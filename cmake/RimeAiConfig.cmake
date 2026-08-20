# Librim AI Module CMake Configuration
# 处理 AI 模块的依赖和编译配置

# ============================================================
# SQLite 配置
# ============================================================

if(ENABLE_AI_MODULE)
    message(STATUS "Configuring Librim AI Module...")
    
    # 查找 SQLite3
    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        message(STATUS "Found SQLite3: ${SQLite3_INCLUDE_DIRS}")
        set(RIME_AI_SQLITE_LIBS SQLite3::SQLite3)
    else()
        # 尝试使用 pkg-config
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SQLITE3 sqlite3)
            if(SQLITE3_FOUND)
                message(STATUS "Found SQLite3 via pkg-config")
                set(RIME_AI_SQLITE_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIRS})
                set(RIME_AI_SQLITE_LIBS ${SQLITE3_LIBRARIES})
            endif()
        endif()
    endif()
    
    if(NOT SQLite3_FOUND AND NOT SQLITE3_FOUND)
        message(WARNING "SQLite3 not found, AI module will use embedded stub")
        set(RIME_AI_SQLITE_LIBS "")
    endif()
    
    # ============================================================
    # SQLCipher 配置（可选）
    # ============================================================
    
    option(USE_SQLCIPHER "Use SQLCipher for encrypted storage" OFF)
    
    if(USE_SQLCIPHER)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SQLCIPHER sqlcipher)
            if(SQLCIPHER_FOUND)
                message(STATUS "Found SQLCipher, enabling encrypted storage")
                add_definitions(-DUSE_SQLCIPHER)
                set(RIME_AI_SQLITE_LIBS ${SQLCIPHER_LIBRARIES})
                set(RIME_AI_SQLITE_INCLUDE_DIRS ${SQLCIPHER_INCLUDE_DIRS})
            else()
                message(WARNING "SQLCipher requested but not found, falling back to SQLite3")
            endif()
        endif()
    endif()
    
    # ============================================================
    # JNI 配置（Android 构建）
    # ============================================================
    
    if(ANDROID)
        message(STATUS "Configuring AI module for Android")
        # JNI 头文件由 Android NDK 提供
        # liblog 用于 Android 日志
        set(RIME_AI_JNI_LIBS log)
    endif()
    
    if(NOT ANDROID)
        # 桌面端构建：llm_jni.h 需要 jni.h，优先从 JAVA_HOME 直接获取
        # （find_package(JNI) 对 JBR 等非标准 JDK 识别不佳）
        if(DEFINED ENV{JAVA_HOME} AND EXISTS "$ENV{JAVA_HOME}/include/jni.h")
            include_directories("$ENV{JAVA_HOME}/include" "$ENV{JAVA_HOME}/include/darwin")
            message(STATUS "JNI headers from JAVA_HOME: $ENV{JAVA_HOME}")
        else()
            find_package(JNI QUIET)
            if(JNI_FOUND)
                message(STATUS "Found JNI (desktop): ${JNI_INCLUDE_DIRS}")
                include_directories(${JNI_INCLUDE_DIRS})
            else()
                message(WARNING "JNI not found on desktop; set JAVA_HOME to a JDK for llm_jni.cc")
            endif()
        endif()
    endif()
    
    # ============================================================
    # 编译定义
    # ============================================================
    
    add_definitions(-DRIME_AI_MODULE_ENABLED)
    
    # ============================================================
    # 输出状态
    # ============================================================
    
    message(STATUS "AI Module Configuration:")
    message(STATUS "  - SQLite3: ${SQLite3_FOUND}")
    message(STATUS "  - SQLCipher: ${USE_SQLCIPHER}")
    message(STATUS "  - Android: ${ANDROID}")
    
endif(ENABLE_AI_MODULE)