//
// Copyright Librim AI Developers
// Distributed under the BSD License
//
// 2026-03-08 Librim AI Team
//
// 特征存储层
// 使用 SQLite + SQLCipher 实现加密存储
//

#ifndef RIME_AI_FEATURE_STORAGE_H_
#define RIME_AI_FEATURE_STORAGE_H_

#include "feature_types.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace rime {
namespace ai {

// ============================================================
// 前向声明
// ============================================================

struct sqlite3;
struct sqlite3_stmt;

// ============================================================
// 存储配置
// ============================================================

struct StorageConfig {
  std::string db_path;           // 数据库文件路径
  std::string encryption_key;    // 加密密钥（空则不加密）
  int max_features = 1000;       // 最大特征数量
  int expire_days = 90;          // 特征过期天数
  bool auto_cleanup = true;      // 自动清理过期特征
};

// ============================================================
// 存储状态
// ============================================================

enum class StorageStatus {
  kClosed,       // 已关闭
  kOpen,         // 已打开
  kError         // 错误状态
};

// ============================================================
// 查询选项
// ============================================================

struct FeatureQueryOptions {
  std::string category;          // 按类别筛选（空=全部）
  double min_confidence = 0.0;   // 最小置信度
  int limit = 100;               // 返回数量限制
  int offset = 0;                // 偏移量（分页）
  bool enabled_only = false;     // 只返回启用的特征
  std::string sort_by;           // 排序字段: "created_at", "updated_at", "confidence"
  bool sort_desc = true;         // 是否降序
};

// ============================================================
// FeatureStorage - 特征存储类
// ============================================================

class FeatureStorage {
public:
  // 获取单例实例
  static FeatureStorage& Instance();
  
  // 打开/关闭存储
  bool Open(const StorageConfig& config);
  void Close();
  
  // 状态查询
  StorageStatus GetStatus() const;
  bool IsOpen() const;
  std::string GetLastError() const;
  
  // ============================================================
  // CRUD 操作
  // ============================================================
  
  // 保存特征（新增或更新）
  bool SaveFeature(const PersonalFeature& feature);
  
  // 批量保存
  bool SaveFeatures(const std::vector<PersonalFeature>& features);
  
  // 获取单个特征
  bool GetFeature(const std::string& id, PersonalFeature* out_feature);
  
  // 查询特征列表
  std::vector<PersonalFeature> QueryFeatures(const FeatureQueryOptions& options);
  
  // 按类别获取特征
  std::vector<PersonalFeature> GetFeaturesByCategory(FeatureCategory category);
  
  // 搜索特征（按键名或值模糊匹配）
  std::vector<PersonalFeature> SearchFeatures(const std::string& keyword);
  
  // 删除特征
  bool DeleteFeature(const std::string& id);
  
  // 删除类别下所有特征
  bool DeleteFeaturesByCategory(FeatureCategory category);
  
  // 清空所有特征
  bool ClearAllFeatures();
  
  // ============================================================
  // 统计与维护
  // ============================================================
  
  // 获取特征总数
  int GetFeatureCount();
  
  // 获取各类别特征数量
  std::vector<std::pair<FeatureCategory, int>> GetFeatureCountByCategory();
  
  // 清理过期特征
  int CleanupExpiredFeatures();
  
  // 数据库优化（VACUUM）
  bool Optimize();
  
  // 导出数据（JSON 格式）
  std::string ExportToJson();
  
  // 导入数据（JSON 格式）
  bool ImportFromJson(const std::string& json);
  
  // ============================================================
  // 事务支持
  // ============================================================
  
  bool BeginTransaction();
  bool Commit();
  bool Rollback();
  
private:
  FeatureStorage();
  ~FeatureStorage();
  
  // 禁止拷贝
  FeatureStorage(const FeatureStorage&) = delete;
  FeatureStorage& operator=(const FeatureStorage&) = delete;
  
  // 内部方法
  bool CreateTable();
  bool CreateIndexes();
  bool PrepareStatements();
  void FinalizeStatements();
  
  // SQL 执行辅助
  bool ExecuteSQL(const std::string& sql);
  bool BindString(sqlite3_stmt* stmt, int index, const std::string& value);
  bool BindInt(sqlite3_stmt* stmt, int index, int value);
  bool BindDouble(sqlite3_stmt* stmt, int index, double value);
  bool BindInt64(sqlite3_stmt* stmt, int index, int64_t value);
  
  // 结果解析
  PersonalFeature ParseFeature(sqlite3_stmt* stmt);
  
  // 成员变量
  sqlite3* db_ = nullptr;
  StorageConfig config_;
  StorageStatus status_ = StorageStatus::kClosed;
  std::string last_error_;
  std::mutex mutex_;
  
  // 预编译语句
  sqlite3_stmt* stmt_insert_ = nullptr;
  sqlite3_stmt* stmt_update_ = nullptr;
  sqlite3_stmt* stmt_select_ = nullptr;
  sqlite3_stmt* stmt_delete_ = nullptr;
  sqlite3_stmt* stmt_query_ = nullptr;
};

// ============================================================
// StorageMigration - 数据库迁移工具
// ============================================================

class StorageMigration {
public:
  // 获取当前数据库版本
  static int GetVersion(sqlite3* db);
  
  // 执行迁移
  static bool Migrate(sqlite3* db, int target_version);
  
private:
  static bool MigrateToV1(sqlite3* db);
  static bool MigrateToV2(sqlite3* db);
  // 后续版本迁移方法...
};

}  // namespace ai
}  // namespace rime

#endif  // RIME_AI_FEATURE_STORAGE_H_