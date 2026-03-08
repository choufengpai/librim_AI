//
// Copyright Librim AI Developers
// Distributed under the BSD License
//
// 2026-03-08 Librim AI Team
//
// 特征存储层实现
//

#include "feature_storage.h"
#include <chrono>
#include <cstring>
#include <sstream>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "LibrimAI_Storage"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...)
#define LOGE(...)
#define LOGW(...)
#endif

// SQLite 头文件
#ifdef USE_SQLCIPHER
#include <sqlcipher/sqlite3.h>
#else
#include <sqlite3.h>
#endif

namespace rime {
namespace ai {

// ============================================================
// 数据库版本
// ============================================================

constexpr int kDBVersion = 1;

// ============================================================
// SQL 语句
// ============================================================

namespace sql {

constexpr const char* kCreateTable = R"(
CREATE TABLE IF NOT EXISTS features (
    id TEXT PRIMARY KEY,
    category INTEGER NOT NULL,
    key TEXT NOT NULL,
    value TEXT NOT NULL,
    confidence REAL DEFAULT 0.0,
    source_text TEXT,
    enabled INTEGER DEFAULT 1,
    created_at INTEGER,
    updated_at INTEGER
);
)";

constexpr const char* kCreateIndexes[] = {
    "CREATE INDEX IF NOT EXISTS idx_category ON features(category);",
    "CREATE INDEX IF NOT EXISTS idx_created ON features(created_at);",
    "CREATE INDEX IF NOT EXISTS idx_key ON features(key);",
    "CREATE INDEX IF NOT EXISTS idx_enabled ON features(enabled);",
};

constexpr const char* kInsert = R"(
INSERT OR REPLACE INTO features 
(id, category, key, value, confidence, source_text, enabled, created_at, updated_at)
VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
)";

constexpr const char* kUpdate = R"(
UPDATE features SET 
    category = ?, key = ?, value = ?, confidence = ?, 
    source_text = ?, enabled = ?, updated_at = ?
WHERE id = ?;
)";

constexpr const char* kSelect = R"(
SELECT id, category, key, value, confidence, source_text, enabled, created_at, updated_at
FROM features WHERE id = ?;
)";

constexpr const char* kDelete = "DELETE FROM features WHERE id = ?;";

constexpr const char* kQuery = R"(
SELECT id, category, key, value, confidence, source_text, enabled, created_at, updated_at
FROM features WHERE 1=1
)";

constexpr const char* kCountByCategory = R"(
SELECT category, COUNT(*) FROM features GROUP BY category;
)";

constexpr const char* kCleanupExpired = R"(
DELETE FROM features WHERE created_at < ?;
)";

}  // namespace sql

// ============================================================
// FeatureStorage 单例实现
// ============================================================

FeatureStorage& FeatureStorage::Instance() {
  static FeatureStorage instance;
  return instance;
}

FeatureStorage::FeatureStorage() = default;

FeatureStorage::~FeatureStorage() {
  Close();
}

// ============================================================
// 打开/关闭存储
// ============================================================

bool FeatureStorage::Open(const StorageConfig& config) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (status_ == StorageStatus::kOpen) {
    LOGI("Storage already open");
    return true;
  }
  
  config_ = config;
  
  // 打开数据库
  int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
  int result = sqlite3_open_v2(config.db_path.c_str(), &db_, flags, nullptr);
  
  if (result != SQLITE_OK) {
    last_error_ = sqlite3_errmsg(db_);
    LOGE("Failed to open database: %s", last_error_.c_str());
    sqlite3_close(db_);
    db_ = nullptr;
    status_ = StorageStatus::kError;
    return false;
  }
  
  // 如果有加密密钥，设置加密
  #ifdef USE_SQLCIPHER
  if (!config.encryption_key.empty()) {
    std::string key_sql = "PRAGMA key = '" + config.encryption_key + "';";
    result = sqlite3_exec(db_, key_sql.c_str(), nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
      last_error_ = "Failed to set encryption key";
      LOGE("%s", last_error_.c_str());
      sqlite3_close(db_);
      db_ = nullptr;
      status_ = StorageStatus::kError;
      return false;
    }
    
    // 验证加密是否成功
    result = sqlite3_exec(db_, "SELECT count(*) FROM sqlite_master;", 
                          nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
      last_error_ = "Invalid encryption key or corrupted database";
      LOGE("%s", last_error_.c_str());
      sqlite3_close(db_);
      db_ = nullptr;
      status_ = StorageStatus::kError;
      return false;
    }
  }
  #endif
  
  // 创建表和索引
  if (!CreateTable() || !CreateIndexes()) {
    sqlite3_close(db_);
    db_ = nullptr;
    status_ = StorageStatus::kError;
    return false;
  }
  
  // 预编译语句
  if (!PrepareStatements()) {
    sqlite3_close(db_);
    db_ = nullptr;
    status_ = StorageStatus::kError;
    return false;
  }
  
  status_ = StorageStatus::kOpen;
  LOGI("Storage opened successfully: %s", config.db_path.c_str());
  return true;
}

void FeatureStorage::Close() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (status_ != StorageStatus::kOpen || !db_) {
    return;
  }
  
  FinalizeStatements();
  
  int result = sqlite3_close(db_);
  if (result != SQLITE_OK) {
    LOGW("Warning: database close returned %d", result);
  }
  
  db_ = nullptr;
  status_ = StorageStatus::kClosed;
  LOGI("Storage closed");
}

// ============================================================
// 状态查询
// ============================================================

StorageStatus FeatureStorage::GetStatus() const {
  return status_;
}

bool FeatureStorage::IsOpen() const {
  return status_ == StorageStatus::kOpen && db_ != nullptr;
}

std::string FeatureStorage::GetLastError() const {
  return last_error_;
}

// ============================================================
// CRUD 操作
// ============================================================

bool FeatureStorage::SaveFeature(const PersonalFeature& feature) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!IsOpen()) {
    last_error_ = "Storage not open";
    return false;
  }
  
  int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  
  int64_t created_at = feature.created_at > 0 ? feature.created_at : now;
  int64_t updated_at = now;
  
  sqlite3_stmt* stmt = stmt_insert_;
  sqlite3_reset(stmt);
  
  int idx = 1;
  sqlite3_bind_text(stmt, idx++, feature.id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, idx++, static_cast<int>(feature.category));
  sqlite3_bind_text(stmt, idx++, feature.key.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, idx++, feature.value.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_double(stmt, idx++, feature.confidence);
  sqlite3_bind_text(stmt, idx++, feature.source_text.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, idx++, feature.enabled ? 1 : 0);
  sqlite3_bind_int64(stmt, idx++, created_at);
  sqlite3_bind_int64(stmt, idx++, updated_at);
  
  int result = sqlite3_step(stmt);
  if (result != SQLITE_DONE) {
    last_error_ = sqlite3_errmsg(db_);
    LOGE("Failed to save feature: %s", last_error_.c_str());
    return false;
  }
  
  return true;
}

bool FeatureStorage::SaveFeatures(const std::vector<PersonalFeature>& features) {
  if (!BeginTransaction()) return false;
  
  for (const auto& feature : features) {
    if (!SaveFeature(feature)) {
      Rollback();
      return false;
    }
  }
  
  return Commit();
}

bool FeatureStorage::GetFeature(const std::string& id, PersonalFeature* out_feature) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!IsOpen() || !out_feature) {
    return false;
  }
  
  sqlite3_stmt* stmt = stmt_select_;
  sqlite3_reset(stmt);
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
  
  int result = sqlite3_step(stmt);
  if (result == SQLITE_ROW) {
    *out_feature = ParseFeature(stmt);
    return true;
  }
  
  return false;
}

std::vector<PersonalFeature> FeatureStorage::QueryFeatures(const FeatureQueryOptions& options) {
  std::vector<PersonalFeature> results;
  
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!IsOpen()) {
    return results;
  }
  
  // 构建查询 SQL
  std::ostringstream sql;
  sql << sql::kQuery;
  
  if (!options.category.empty()) {
    sql << " AND category = ?";
  }
  if (options.min_confidence > 0.0) {
    sql << " AND confidence >= ?";
  }
  if (options.enabled_only) {
    sql << " AND enabled = 1";
  }
  
  // 排序
  std::string sort_field = "created_at";
  if (options.sort_by == "updated_at" || options.sort_by == "confidence") {
    sort_field = options.sort_by;
  }
  sql << " ORDER BY " << sort_field << (options.sort_desc ? " DESC" : " ASC");
  
  // 分页
  sql << " LIMIT ? OFFSET ?";
  
  // 准备语句
  sqlite3_stmt* stmt = nullptr;
  int result = sqlite3_prepare_v2(db_, sql.str().c_str(), -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    last_error_ = sqlite3_errmsg(db_);
    LOGE("Failed to prepare query: %s", last_error_.c_str());
    return results;
  }
  
  // 绑定参数
  int idx = 1;
  if (!options.category.empty()) {
    int cat = std::stoi(options.category);
    sqlite3_bind_int(stmt, idx++, cat);
  }
  if (options.min_confidence > 0.0) {
    sqlite3_bind_double(stmt, idx++, options.min_confidence);
  }
  sqlite3_bind_int(stmt, idx++, options.limit);
  sqlite3_bind_int(stmt, idx++, options.offset);
  
  // 执行查询
  while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
    results.push_back(ParseFeature(stmt));
  }
  
  sqlite3_finalize(stmt);
  return results;
}

std::vector<PersonalFeature> FeatureStorage::GetFeaturesByCategory(FeatureCategory category) {
  FeatureQueryOptions options;
  options.category = std::to_string(static_cast<int>(category));
  return QueryFeatures(options);
}

std::vector<PersonalFeature> FeatureStorage::SearchFeatures(const std::string& keyword) {
  std::vector<PersonalFeature> results;
  
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!IsOpen() || keyword.empty()) {
    return results;
  }
  
  std::string sql = std::string(sql::kQuery) + 
      " AND (key LIKE ? OR value LIKE ? OR source_text LIKE ?)";
  
  sqlite3_stmt* stmt = nullptr;
  int result = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    last_error_ = sqlite3_errmsg(db_);
    return results;
  }
  
  std::string pattern = "%" + keyword + "%";
  sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, pattern.c_str(), -1, SQLITE_TRANSIENT);
  
  while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
    results.push_back(ParseFeature(stmt));
  }
  
  sqlite3_finalize(stmt);
  return results;
}

bool FeatureStorage::DeleteFeature(const std::string& id) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!IsOpen()) return false;
  
  sqlite3_stmt* stmt = stmt_delete_;
  sqlite3_reset(stmt);
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
  
  int result = sqlite3_step(stmt);
  if (result != SQLITE_DONE) {
    last_error_ = sqlite3_errmsg(db_);
    return false;
  }
  
  return sqlite3_changes(db_) > 0;
}

bool FeatureStorage::DeleteFeaturesByCategory(FeatureCategory category) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!IsOpen()) return false;
  
  std::string sql = "DELETE FROM features WHERE category = ?;";
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
  sqlite3_bind_int(stmt, 1, static_cast<int>(category));
  
  int result = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  
  return result == SQLITE_DONE;
}

bool FeatureStorage::ClearAllFeatures() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!IsOpen()) return false;
  
  return ExecuteSQL("DELETE FROM features;");
}

// ============================================================
// 统计与维护
// ============================================================

int FeatureStorage::GetFeatureCount() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!IsOpen()) return 0;
  
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM features;", -1, &stmt, nullptr);
  
  int count = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    count = sqlite3_column_int(stmt, 0);
  }
  
  sqlite3_finalize(stmt);
  return count;
}

std::vector<std::pair<FeatureCategory, int>> FeatureStorage::GetFeatureCountByCategory() {
  std::vector<std::pair<FeatureCategory, int>> results;
  
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!IsOpen()) return results;
  
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, sql::kCountByCategory, -1, &stmt, nullptr);
  
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int cat = sqlite3_column_int(stmt, 0);
    int count = sqlite3_column_int(stmt, 1);
    results.emplace_back(static_cast<FeatureCategory>(cat), count);
  }
  
  sqlite3_finalize(stmt);
  return results;
}

int FeatureStorage::CleanupExpiredFeatures() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!IsOpen() || config_.expire_days <= 0) return 0;
  
  int64_t expire_threshold = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count() 
      - (config_.expire_days * 24 * 60 * 60);
  
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, sql::kCleanupExpired, -1, &stmt, nullptr);
  sqlite3_bind_int64(stmt, 1, expire_threshold);
  
  int result = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  
  if (result != SQLITE_DONE) {
    return 0;
  }
  
  return sqlite3_changes(db_);
}

bool FeatureStorage::Optimize() {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!IsOpen()) return false;
  
  return ExecuteSQL("VACUUM;");
}

std::string FeatureStorage::ExportToJson() {
  std::ostringstream json;
  json << "{\"features\":[";
  
  auto features = QueryFeatures(FeatureQueryOptions{});
  
  for (size_t i = 0; i < features.size(); ++i) {
    if (i > 0) json << ",";
    const auto& f = features[i];
    json << "{\"id\":\"" << f.id 
         << "\",\"category\":" << static_cast<int>(f.category)
         << ",\"key\":\"" << f.key 
         << "\",\"value\":\"" << f.value 
         << "\",\"confidence\":" << f.confidence
         << ",\"enabled\":" << (f.enabled ? "true" : "false") << "}";
  }
  
  json << "]}";
  return json.str();
}

bool FeatureStorage::ImportFromJson(const std::string& json) {
  // TODO: 实现 JSON 解析和导入
  // 需要添加 JSON 库依赖
  return false;
}

// ============================================================
// 事务支持
// ============================================================

bool FeatureStorage::BeginTransaction() {
  return ExecuteSQL("BEGIN TRANSACTION;");
}

bool FeatureStorage::Commit() {
  return ExecuteSQL("COMMIT;");
}

bool FeatureStorage::Rollback() {
  return ExecuteSQL("ROLLBACK;");
}

// ============================================================
// 内部方法
// ============================================================

bool FeatureStorage::CreateTable() {
  return ExecuteSQL(sql::kCreateTable);
}

bool FeatureStorage::CreateIndexes() {
  for (const char* sql : sql::kCreateIndexes) {
    if (!ExecuteSQL(sql)) {
      return false;
    }
  }
  return true;
}

bool FeatureStorage::PrepareStatements() {
  int result;
  
  result = sqlite3_prepare_v2(db_, sql::kInsert, -1, &stmt_insert_, nullptr);
  if (result != SQLITE_OK) {
    last_error_ = "Failed to prepare insert statement";
    return false;
  }
  
  result = sqlite3_prepare_v2(db_, sql::kUpdate, -1, &stmt_update_, nullptr);
  if (result != SQLITE_OK) {
    last_error_ = "Failed to prepare update statement";
    return false;
  }
  
  result = sqlite3_prepare_v2(db_, sql::kSelect, -1, &stmt_select_, nullptr);
  if (result != SQLITE_OK) {
    last_error_ = "Failed to prepare select statement";
    return false;
  }
  
  result = sqlite3_prepare_v2(db_, sql::kDelete, -1, &stmt_delete_, nullptr);
  if (result != SQLITE_OK) {
    last_error_ = "Failed to prepare delete statement";
    return false;
  }
  
  return true;
}

void FeatureStorage::FinalizeStatements() {
  if (stmt_insert_) sqlite3_finalize(stmt_insert_);
  if (stmt_update_) sqlite3_finalize(stmt_update_);
  if (stmt_select_) sqlite3_finalize(stmt_select_);
  if (stmt_delete_) sqlite3_finalize(stmt_delete_);
  if (stmt_query_) sqlite3_finalize(stmt_query_);
  
  stmt_insert_ = nullptr;
  stmt_update_ = nullptr;
  stmt_select_ = nullptr;
  stmt_delete_ = nullptr;
  stmt_query_ = nullptr;
}

bool FeatureStorage::ExecuteSQL(const std::string& sql) {
  char* error_msg = nullptr;
  int result = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);
  
  if (result != SQLITE_OK) {
    last_error_ = error_msg ? error_msg : "Unknown error";
    LOGE("SQL error: %s", last_error_.c_str());
    if (error_msg) sqlite3_free(error_msg);
    return false;
  }
  
  return true;
}

PersonalFeature FeatureStorage::ParseFeature(sqlite3_stmt* stmt) {
  PersonalFeature feature;
  
  feature.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
  feature.category = static_cast<FeatureCategory>(sqlite3_column_int(stmt, 1));
  feature.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
  feature.value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
  feature.confidence = sqlite3_column_double(stmt, 4);
  
  const char* source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
  if (source) feature.source_text = source;
  
  feature.enabled = sqlite3_column_int(stmt, 6) != 0;
  feature.created_at = sqlite3_column_int64(stmt, 7);
  feature.updated_at = sqlite3_column_int64(stmt, 8);
  
  return feature;
}

// ============================================================
// StorageMigration 实现
// ============================================================

int StorageMigration::GetVersion(sqlite3* db) {
  if (!db) return 0;
  
  sqlite3_stmt* stmt = nullptr;
  int result = sqlite3_prepare_v2(db, 
      "PRAGMA user_version;", -1, &stmt, nullptr);
  
  if (result != SQLITE_OK) return 0;
  
  int version = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    version = sqlite3_column_int(stmt, 0);
  }
  
  sqlite3_finalize(stmt);
  return version;
}

bool StorageMigration::Migrate(sqlite3* db, int target_version) {
  int current = GetVersion(db);
  
  while (current < target_version) {
    bool success = false;
    
    switch (current + 1) {
      case 1:
        success = MigrateToV1(db);
        break;
      // 后续版本迁移...
      default:
        return false;
    }
    
    if (!success) return false;
    current = GetVersion(db);
  }
  
  return true;
}

bool StorageMigration::MigrateToV1(sqlite3* db) {
  // V1: 初始表结构
  // 表已在 CreateTables 中创建
  
  char* error_msg = nullptr;
  int result = sqlite3_exec(db, "PRAGMA user_version = 1;", 
                            nullptr, nullptr, &error_msg);
  
  if (error_msg) sqlite3_free(error_msg);
  return result == SQLITE_OK;
}

}  // namespace ai
}  // namespace rime