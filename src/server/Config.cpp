/**
 * @file server/Config.cpp
 * @date 03.07.2025
 *
 * @author Urs Behrmann
 *
 * @brief Configuration loading implementation (loadConfig).
 */

#include "../Geruest.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "modules/ModuleHooks.hpp"

namespace geruest {

void Geruest::loadConfig(const std::string& envFilePath) {
    sendToLogger("Loading configuration from environment...");
    
    // Load .env file (if it exists)
    ConfigLoader::loadEnvFile(envFilePath);
    
    // Apply configuration values only if NOT explicitly set via code
    // Hierarchy: Code > .env > environment variables
    
    // PORT
    if (!_configFlags.portSet) {
        int configPort = ConfigLoader::getInt("PORT", port);
        if (configPort != port) {
            port = configPort;
            sendToLogger("PORT loaded from config: " + std::to_string(port));
        }
    }
    
    // HOSTNAME
    if (!_configFlags.hostnameSet) {
        std::string configHostname = ConfigLoader::get("HOSTNAME", hostname_);
        if (configHostname != hostname_) {
            hostname_ = configHostname;
            sendToLogger("HOSTNAME loaded from config: " + hostname_);
        }
    }

    // BIND_ADDRESS
    if (!_configFlags.bindAddressSet) {
        std::string configBindAddress = ConfigLoader::get("BIND_ADDRESS", bindAddress_);
        if (configBindAddress != bindAddress_) {
            bindAddress_ = configBindAddress;
            sendToLogger("BIND_ADDRESS loaded from config: " + bindAddress_);
        }
    }
    
    // WEBP_CONVERSION
    if (!_configFlags.webpConversionSet) {
        bool configWebP = ConfigLoader::getBool("WEBP_CONVERSION", false);
        if (configWebP) {
#if GERUEST_HAS_WEBP
            serverData.setWebPConversion(true);
            sendToLogger("WEBP_CONVERSION enabled from config");
#else
            sendToLoggerError("WEBP_CONVERSION cannot be enabled - library not available");
#endif
        }
    }
    
    // WEBP_QUALITY
    if (!_configFlags.webpQualitySet) {
        float configQuality = ConfigLoader::getFloat("WEBP_QUALITY", 75.0f);
        if (configQuality >= 0.0f && configQuality <= 100.0f) {
#if GERUEST_HAS_WEBP
            serverData.setWebPQuality(configQuality);
            sendToLogger("WEBP_QUALITY loaded from config: " + std::to_string(static_cast<int>(configQuality)) + "%");
#endif
        }
    }
    
    // DEV_MODE
    if (!_configFlags.devModeSet) {
        bool configDevMode = ConfigLoader::getBool("DEV_MODE", false);
        if (configDevMode) {
            serverData.enableDevMode();
            sendToLogger("DEV_MODE enabled from config");
        }
    }
    
    // MERGE_ASSETS
    if (!_configFlags.mergeAssetsSet) {
        bool configMerge = ConfigLoader::getBool("MERGE_ASSETS", false);
        if (configMerge) {
            serverData.setMergeAssets(true);
            sendToLogger("MERGE_ASSETS enabled from config");
        }
    }
    
    // WORKER_THREADS
    if (!_configFlags.workerThreadsSet) {
        size_t configThreads = ConfigLoader::getSizeT("WORKER_THREADS", _workerThreadCount);
        if (configThreads > 0 && configThreads != _workerThreadCount) {
            _workerThreadCount = configThreads;
            sendToLogger("WORKER_THREADS loaded from config: " + std::to_string(_workerThreadCount));
        }
    }
    
    // MAX_QUEUE_SIZE
    if (!_configFlags.maxQueueSizeSet) {
        size_t configQueueSize = ConfigLoader::getSizeT("MAX_QUEUE_SIZE", _maxQueueSize);
        if (configQueueSize > 0 && configQueueSize != _maxQueueSize) {
            _maxQueueSize = configQueueSize;
            sendToLogger("MAX_QUEUE_SIZE (max concurrent sessions) loaded from config: " + std::to_string(_maxQueueSize));
        }
    }

    // MAX_REQUESTS_PER_CONNECTION (0 = unlimited)
    if (!_configFlags.maxRequestsPerConnectionSet) {
        const size_t currentValue = serverData.getMaxRequestsPerConnection();
        const size_t configValue = ConfigLoader::getSizeT("MAX_REQUESTS_PER_CONNECTION", currentValue);
        if (configValue != currentValue) {
            serverData.setMaxRequestsPerConnection(configValue);
            if (configValue == 0) {
                sendToLogger("MAX_REQUESTS_PER_CONNECTION loaded from config: unlimited");
            } else {
                sendToLogger("MAX_REQUESTS_PER_CONNECTION loaded from config: " + std::to_string(configValue));
            }
        }
    }

    // TEXT_RESPONSE_CACHE_MAX_ENTRY_BYTES (0 = do not cache new entries; ignored in dev mode)
    if (!_configFlags.textResponseCacheMaxEntryBytesSet && !serverData.isDevMode()) {
        const size_t currentValue = serverData.getTextResponseCacheMaxEntryBytes();
        const size_t configValue = ConfigLoader::getSizeT("TEXT_RESPONSE_CACHE_MAX_ENTRY_BYTES", currentValue);
        if (configValue != currentValue) {
            serverData.setTextResponseCacheMaxEntryBytes(configValue);
            sendToLogger("TEXT_RESPONSE_CACHE_MAX_ENTRY_BYTES loaded from config: " + std::to_string(configValue));
        }
    }

    // TEXT_RESPONSE_CACHE_MAX_TOTAL_BYTES (0 = do not cache new entries; ignored in dev mode)
    if (!_configFlags.textResponseCacheMaxTotalBytesSet && !serverData.isDevMode()) {
        const size_t currentValue = serverData.getTextResponseCacheMaxTotalBytes();
        const size_t configValue = ConfigLoader::getSizeT("TEXT_RESPONSE_CACHE_MAX_TOTAL_BYTES", currentValue);
        if (configValue != currentValue) {
            serverData.setTextResponseCacheMaxTotalBytes(configValue);
            sendToLogger("TEXT_RESPONSE_CACHE_MAX_TOTAL_BYTES loaded from config: " + std::to_string(configValue));
        }
    }
    
    // LOG_LEVEL
    if (!_configFlags.logLevelSet) {
        std::string configLogLevel = ConfigLoader::get("LOG_LEVEL", "");
        if (!configLogLevel.empty()) {
            LogLevel level = LogLevel::Error;
            std::string levelLower = configLogLevel;
            std::transform(levelLower.begin(), levelLower.end(), levelLower.begin(), ::tolower);
            
            if (levelLower == "none") {
                level = LogLevel::None;
            } else if (levelLower == "error") {
                level = LogLevel::Error;
            } else if (levelLower == "warning" || levelLower == "warn") {
                level = LogLevel::Warning;
            } else if (levelLower == "info") {
                level = LogLevel::Info;
            } else if (levelLower == "debug") {
                level = LogLevel::Debug;
            } else {
                sendToLoggerError("Invalid LOG_LEVEL in config: " + configLogLevel + " (using default: Error)");
            }
            
            serverData.setLogLevel(level);
            sendToLogger("LOG_LEVEL loaded from config: " + configLogLevel);
        }
    }
    
#if GERUEST_HAS_CURL
    if (const auto& applier = modules::emailConfigApplier()) {
        applier(*this);
    }
#endif

    {
        auto trimToken = [](std::string w) -> std::string {
            size_t a = 0;
            while (a < w.size() && std::isspace(static_cast<unsigned char>(w[a]))) {
                ++a;
            }
            size_t b = w.size();
            while (b > a && std::isspace(static_cast<unsigned char>(w[b - 1]))) {
                --b;
            }
            return w.substr(a, b - a);
        };
        auto consumeCommaList = [&](const std::string& s, const auto& onToken) {
            size_t i = 0;
            while (i < s.size()) {
                while (i < s.size()
                       && (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == ',')) {
                    ++i;
                }
                if (i >= s.size()) {
                    break;
                }
                size_t j = i;
                while (j < s.size() && s[j] != ',') {
                    ++j;
                }
                std::string w = trimToken(s.substr(i, j - i));
                if (!w.empty()) {
                    onToken(w);
                }
                i = j + 1;
            }
        };

        std::string preserveList = ConfigLoader::get("OBFUSCATE_PRESERVE", "");
        if (!preserveList.empty()) {
            consumeCommaList(preserveList,
                             [this](const std::string& w) { serverData.addObfuscationPreserveIdent(w); });
            sendToLogger("OBFUSCATE_PRESERVE applied");
        }
        std::string externList = ConfigLoader::get("OBFUSCATE_EXTERNS", "");
        if (!externList.empty()) {
            consumeCommaList(externList,
                             [this](const std::string& w) { serverData.addObfuscationExternGlobal(w); });
            sendToLogger("OBFUSCATE_EXTERNS applied");
        }
        std::string externFile = ConfigLoader::get("OBFUSCATE_EXTERNS_FILE", "");
        if (!externFile.empty()) {
            loadObfuscationExternsFile(externFile);
        }
        if (ConfigLoader::getBool("OBFUSCATE_STRICT_UNDEFINED", false)) {
            serverData.setObfuscationStrictUndefined(true);
            sendToLogger("OBFUSCATE_STRICT_UNDEFINED enabled");
        }
        if (ConfigLoader::getBool("OBFUSCATE_EMIT_GLOBALTHIS", false)) {
            serverData.setObfuscationEmitGlobalThisAssignments(true);
            sendToLogger("OBFUSCATE_EMIT_GLOBALTHIS enabled");
        }
        if (ConfigLoader::getBool("OBFUSCATE_VALIDATE_ACORN", false)) {
            serverData.setObfuscationValidateWithAcorn(true);
            sendToLogger("OBFUSCATE_VALIDATE_ACORN enabled");
        }
        serverData.setObfuscationAutoBracketKeys(
            ConfigLoader::getBool("OBFUSCATE_AUTO_BRACKET_KEYS", true));
        if (!serverData.getObfuscationAutoBracketKeys()) {
            sendToLogger("OBFUSCATE_AUTO_BRACKET_KEYS disabled");
        }
    }

    if (!_configFlags.databaseBackendSet) {
        std::string backend = ConfigLoader::get("DATABASE_BACKEND", "none");
        std::transform(backend.begin(), backend.end(), backend.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (backend == "postgres") {
            _databaseBackend = DatabaseBackend::Postgres;
        } else if (backend == "sqlite") {
            _databaseBackend = DatabaseBackend::Sqlite;
        } else {
            _databaseBackend = DatabaseBackend::None;
        }
    }

    if (!_configFlags.databasePoolSizeSet) {
        _dbCommonConfig.poolSize = ConfigLoader::getSizeT("DATABASE_POOL_MAX", _dbCommonConfig.poolSize);
    }
    if (!_configFlags.sqliteExecutorThreadsSet) {
        _dbCommonConfig.sqliteExecutorThreads =
            ConfigLoader::getSizeT("SQLITE_DB_EXECUTOR_THREADS", _dbCommonConfig.sqliteExecutorThreads);
    }

#if GERUEST_HAS_LIBPQ
    if (!_configFlags.postgresConfigSet) {
        _postgresConfig.host = ConfigLoader::get("POSTGRES_HOST", _postgresConfig.host);
        _postgresConfig.port = ConfigLoader::getInt("POSTGRES_PORT", _postgresConfig.port);
        _postgresConfig.database = ConfigLoader::get("POSTGRES_DB", _postgresConfig.database);
        _postgresConfig.user = ConfigLoader::get("POSTGRES_USER", _postgresConfig.user);
        _postgresConfig.password = ConfigLoader::get("POSTGRES_PASSWORD", _postgresConfig.password);
        _postgresConfig.sslmode = ConfigLoader::get("POSTGRES_SSLMODE", _postgresConfig.sslmode);
        _postgresConfig.connectTimeoutSeconds =
            ConfigLoader::getInt("POSTGRES_CONNECT_TIMEOUT", _postgresConfig.connectTimeoutSeconds);
        _postgresConfig.statementTimeoutMs =
            ConfigLoader::getInt("POSTGRES_STATEMENT_TIMEOUT_MS", _postgresConfig.statementTimeoutMs);
        const int pipeBatch = ConfigLoader::getInt("POSTGRES_PIPELINE_MAX_BATCH", static_cast<int>(_postgresConfig.maxPipelineBatch));
        if (pipeBatch < 1) {
            _postgresConfig.maxPipelineBatch = 1;
        } else if (pipeBatch > 256) {
            _postgresConfig.maxPipelineBatch = 256;
        } else {
            _postgresConfig.maxPipelineBatch = static_cast<unsigned>(pipeBatch);
        }
    }
#endif

#if GERUEST_HAS_SQLITE
    if (!_configFlags.sqliteConfigSet) {
        _sqliteConfig.path = ConfigLoader::get("SQLITE_PATH", _sqliteConfig.path);
        _sqliteConfig.busyTimeoutMs = ConfigLoader::getInt("SQLITE_BUSY_TIMEOUT_MS", _sqliteConfig.busyTimeoutMs);
    }
#endif

    initializeDatabaseFromConfig();

    sendToLogger("Configuration loading complete");
}

void Geruest::initializeDatabaseFromConfig() {
    serverData.setDatabaseClient(nullptr);

    if (_databaseBackend == DatabaseBackend::None) {
        return;
    }

    if (_databaseBackend == DatabaseBackend::Postgres) {
#if GERUEST_HAS_LIBPQ
        if (_postgresConfig.database.empty() || _postgresConfig.user.empty()) {
            sendToLoggerError("PostgreSQL selected but configuration incomplete (POSTGRES_DB/POSTGRES_USER)");
            return;
        }
        serverData.setDatabaseClient(db::createPostgresClient(_postgresConfig, _dbCommonConfig));
        sendToLogger("PostgreSQL backend initialized with pool size " + std::to_string(_dbCommonConfig.poolSize));
        return;
#else
        throw std::runtime_error("DATABASE_BACKEND=postgres selected but Geruest was built without PostgreSQL support");
#endif
    }

    if (_databaseBackend == DatabaseBackend::Sqlite) {
#if GERUEST_HAS_SQLITE
        if (_sqliteConfig.path.empty()) {
            sendToLoggerError("SQLite selected but SQLITE_PATH is empty");
            return;
        }
        serverData.setDatabaseClient(db::createSqliteClient(_sqliteConfig, _dbCommonConfig));
        sendToLogger("SQLite backend initialized with pool size " + std::to_string(_dbCommonConfig.poolSize)
                     + " and executor threads " + std::to_string(_dbCommonConfig.sqliteExecutorThreads));
        return;
#else
        throw std::runtime_error("DATABASE_BACKEND=sqlite selected but Geruest was built without SQLite support");
#endif
    }
}

}  // namespace geruest
