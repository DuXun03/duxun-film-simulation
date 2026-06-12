#ifndef DUXUN_FILM_LICENSE_H
#define DUXUN_FILM_LICENSE_H

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "Advapi32.lib")
#endif

namespace duxun_license {

// ProgramData layout: DuXun\FilmSim\license.json, activation-request.json, logs\license.log.
static const char* kProductId = "duxun-filmsim-ofx-v5";
static const char* kKeyVersion = "v1";
static const int kTrialSeconds = 24 * 60 * 60;
static const int kClockRollbackGraceSeconds = 10 * 60;

static const unsigned char DUXUN_LICENSE_PUBLIC_KEY_V1_X[32] = {
    0x7C, 0x7D, 0x6A, 0xD7, 0x1F, 0x54, 0x26, 0x41,
    0xD3, 0xCB, 0x8F, 0x34, 0x6B, 0x99, 0x9A, 0x34,
    0xCF, 0xC6, 0xE0, 0xF0, 0x0D, 0xA7, 0xCC, 0xB3,
    0x41, 0xEB, 0x08, 0xD5, 0xE6, 0x9E, 0x8D, 0x76
};

static const unsigned char DUXUN_LICENSE_PUBLIC_KEY_V1_Y[32] = {
    0xF7, 0x14, 0xFC, 0x69, 0xF4, 0x9C, 0x1A, 0xC7,
    0x85, 0x5D, 0xC7, 0xDB, 0x40, 0x98, 0x82, 0xF8,
    0x90, 0x3E, 0x13, 0x04, 0xF4, 0xAC, 0xE0, 0xC1,
    0x72, 0xCD, 0x10, 0x1E, 0xE6, 0x55, 0x85, 0x2B
};

struct LicenseDocument {
    std::string productId;
    std::string licenseType;
    std::string issuedAt;
    std::string expiresAt;
    bool expiresAtNull;
    std::string machineHash;
    std::string customerId;
    std::string orderId;
    std::vector<std::string> features;
    std::string signature;
    std::string keyVersion;
};

struct LocalLicenseState {
    bool signedLicenseValid;
    bool trialActive;
    bool watermarkRequired;
    bool machineMismatch;
    bool expired;
    std::string code;
    std::string statusText;
    std::string detail;
};

static std::string trimLower(std::string value) {
    while (!value.empty() && std::isspace((unsigned char)value.front())) value.erase(value.begin());
    while (!value.empty() && std::isspace((unsigned char)value.back())) value.pop_back();
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return (char)std::tolower(ch);
    });
    return value;
}

static std::string getEnvString(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

static std::string joinPath(const std::string& base, const std::string& child) {
    if (base.empty()) return child;
    char last = base[base.size() - 1];
    if (last == '\\' || last == '/') return base + child;
    return base + "\\" + child;
}

static std::string rootPath() {
    std::string overridePath = getEnvString("DUXUN_FILMSIM_LICENSE_DIR");
    if (!overridePath.empty()) return overridePath;
    std::string programData = getEnvString("ProgramData");
    if (programData.empty()) programData = getEnvString("PROGRAMDATA");
    if (programData.empty()) programData = "C:\\ProgramData";
    return joinPath(programData, "DuXun\\FilmSim");
}

static std::string licensePath() { return joinPath(rootPath(), "license.json"); }
static std::string trialPath() { return joinPath(rootPath(), "trial.json"); }
static std::string activationRequestPath() { return joinPath(rootPath(), "activation-request.json"); }
static std::string licenseLogPath() { return joinPath(joinPath(rootPath(), "logs"), "license.log"); }

static bool ensureDirectory(const std::string& path) {
#ifdef _WIN32
    if (path.empty()) return false;
    std::string partial;
    for (size_t i = 0; i < path.size(); ++i) {
        char ch = path[i];
        partial.push_back(ch);
        if ((ch == '\\' || ch == '/') && partial.size() > 3) {
            CreateDirectoryA(partial.c_str(), nullptr);
        }
    }
    return CreateDirectoryA(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    (void)path;
    return false;
#endif
}

static void ensureLayout() {
    ensureDirectory(rootPath());
    ensureDirectory(joinPath(rootPath(), "logs"));
}

static bool readTextFile(const std::string& path, std::string& output) {
    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input) return false;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    output = buffer.str();
    return true;
}

static bool writeTextFile(const std::string& path, const std::string& text) {
    ensureLayout();
    std::ofstream output(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output << text;
    return !!output;
}

static std::string isoUtcFromUnix(std::time_t value) {
    std::tm tmValue;
#ifdef _WIN32
    gmtime_s(&tmValue, &value);
#else
    gmtime_r(&value, &tmValue);
#endif
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tmValue.tm_year + 1900, tmValue.tm_mon + 1, tmValue.tm_mday,
                  tmValue.tm_hour, tmValue.tm_min, tmValue.tm_sec);
    return std::string(buffer);
}

static bool unixFromIsoUtc(const std::string& text, std::time_t& result) {
    int year = 0, mon = 0, day = 0, hour = 0, min = 0, sec = 0;
    if (std::sscanf(text.c_str(), "%d-%d-%dT%d:%d:%dZ", &year, &mon, &day, &hour, &min, &sec) != 6) {
        return false;
    }
    std::tm tmValue;
    std::memset(&tmValue, 0, sizeof(tmValue));
    tmValue.tm_year = year - 1900;
    tmValue.tm_mon = mon - 1;
    tmValue.tm_mday = day;
    tmValue.tm_hour = hour;
    tmValue.tm_min = min;
    tmValue.tm_sec = sec;
#ifdef _WIN32
    result = _mkgmtime(&tmValue);
#else
    result = timegm(&tmValue);
#endif
    return result != (std::time_t)-1;
}

static std::vector<unsigned char> sha256Bytes(const std::string& text) {
    std::vector<unsigned char> digest(32, 0);
#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectLength = 0, hashLength = 0, cbData = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return digest;
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objectLength, sizeof(objectLength), &cbData, 0);
    BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLength, sizeof(hashLength), &cbData, 0);
    std::vector<unsigned char> object(objectLength);
    std::vector<unsigned char> output(hashLength);
    if (BCryptCreateHash(alg, &hash, object.empty() ? nullptr : object.data(), objectLength, nullptr, 0, 0) >= 0) {
        BCryptHashData(hash, (PUCHAR)text.data(), (ULONG)text.size(), 0);
        BCryptFinishHash(hash, output.data(), hashLength, 0);
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    if (output.size() == 32) return output;
#else
    std::hash<std::string> fallback;
    size_t a = fallback(text);
    size_t b = fallback("duxun" + text);
    for (int i = 0; i < 16; ++i) {
        digest[i] = (unsigned char)((a >> ((i % sizeof(size_t)) * 8)) & 0xff);
        digest[i + 16] = (unsigned char)((b >> ((i % sizeof(size_t)) * 8)) & 0xff);
    }
#endif
    return digest;
}

static std::string hexUpper(const std::vector<unsigned char>& bytes) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0');
    for (size_t i = 0; i < bytes.size(); ++i) out << std::setw(2) << (int)bytes[i];
    return out.str();
}

static std::string sha256HexUpper(const std::string& text) {
    return hexUpper(sha256Bytes(text));
}

static bool base64Decode(const std::string& input, std::vector<unsigned char>& output) {
#ifdef _WIN32
    DWORD decodedLength = 0;
    if (!CryptStringToBinaryA(input.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &decodedLength, nullptr, nullptr)) {
        return false;
    }
    output.assign(decodedLength, 0);
    return CryptStringToBinaryA(input.c_str(), 0, CRYPT_STRING_BASE64, output.data(), &decodedLength, nullptr, nullptr) != 0;
#else
    (void)input;
    output.clear();
    return false;
#endif
}

static std::string jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (size_t i = 0; i < value.size(); ++i) {
        unsigned char ch = (unsigned char)value[i];
        switch (ch) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (ch < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)ch;
                } else {
                    out << value[i];
                }
        }
    }
    return out.str();
}

static void skipWs(const std::string& text, size_t& pos) {
    while (pos < text.size() && std::isspace((unsigned char)text[pos])) ++pos;
}

static bool findJsonValue(const std::string& text, const char* key, size_t& pos) {
    std::string needle = std::string("\"") + key + "\"";
    size_t keyPos = text.find(needle);
    if (keyPos == std::string::npos) return false;
    size_t colon = text.find(':', keyPos + needle.size());
    if (colon == std::string::npos) return false;
    pos = colon + 1;
    skipWs(text, pos);
    return pos < text.size();
}

static bool parseJsonStringAt(const std::string& text, size_t& pos, std::string& value) {
    if (pos >= text.size() || text[pos] != '"') return false;
    ++pos;
    std::ostringstream out;
    while (pos < text.size()) {
        char ch = text[pos++];
        if (ch == '"') {
            value = out.str();
            return true;
        }
        if (ch == '\\' && pos < text.size()) {
            char esc = text[pos++];
            switch (esc) {
                case '"': out << '"'; break;
                case '\\': out << '\\'; break;
                case '/': out << '/'; break;
                case 'b': out << '\b'; break;
                case 'f': out << '\f'; break;
                case 'n': out << '\n'; break;
                case 'r': out << '\r'; break;
                case 't': out << '\t'; break;
                default: out << esc; break;
            }
        } else {
            out << ch;
        }
    }
    return false;
}

static bool extractStringOrNull(const std::string& text, const char* key, std::string& value, bool& isNull) {
    size_t pos = 0;
    if (!findJsonValue(text, key, pos)) return false;
    if (text.compare(pos, 4, "null") == 0) {
        value.clear();
        isNull = true;
        return true;
    }
    isNull = false;
    return parseJsonStringAt(text, pos, value);
}

static bool extractString(const std::string& text, const char* key, std::string& value) {
    bool isNull = false;
    return extractStringOrNull(text, key, value, isNull) && !isNull;
}

static bool extractStringArray(const std::string& text, const char* key, std::vector<std::string>& values) {
    size_t pos = 0;
    if (!findJsonValue(text, key, pos) || text[pos] != '[') return false;
    ++pos;
    values.clear();
    for (;;) {
        skipWs(text, pos);
        if (pos >= text.size()) return false;
        if (text[pos] == ']') return true;
        std::string item;
        if (!parseJsonStringAt(text, pos, item)) return false;
        values.push_back(item);
        skipWs(text, pos);
        if (pos < text.size() && text[pos] == ',') {
            ++pos;
            continue;
        }
        if (pos < text.size() && text[pos] == ']') return true;
        return false;
    }
}

static bool parseLicenseJson(const std::string& json, LicenseDocument& doc) {
    if (!extractString(json, "productId", doc.productId)) return false;
    if (!extractString(json, "licenseType", doc.licenseType)) return false;
    if (!extractString(json, "issuedAt", doc.issuedAt)) return false;
    if (!extractStringOrNull(json, "expiresAt", doc.expiresAt, doc.expiresAtNull)) return false;
    if (!extractString(json, "machineHash", doc.machineHash)) return false;
    if (!extractString(json, "customerId", doc.customerId)) return false;
    if (!extractString(json, "orderId", doc.orderId)) return false;
    if (!extractStringArray(json, "features", doc.features)) return false;
    if (!extractString(json, "signature", doc.signature)) return false;
    if (!extractString(json, "keyVersion", doc.keyVersion)) return false;
    return true;
}

static std::string featureArrayPayload(const std::vector<std::string>& features) {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < features.size(); ++i) {
        if (i) out << ",";
        out << "\"" << jsonEscape(features[i]) << "\"";
    }
    out << "]";
    return out.str();
}

static std::string canonicalLicensePayload(const LicenseDocument& doc) {
    std::ostringstream out;
    out << "{";
    out << "\"customerId\":\"" << jsonEscape(doc.customerId) << "\",";
    out << "\"expiresAt\":";
    if (doc.expiresAtNull) out << "null,";
    else out << "\"" << jsonEscape(doc.expiresAt) << "\",";
    out << "\"features\":" << featureArrayPayload(doc.features) << ",";
    out << "\"issuedAt\":\"" << jsonEscape(doc.issuedAt) << "\",";
    out << "\"keyVersion\":\"" << jsonEscape(doc.keyVersion) << "\",";
    out << "\"licenseType\":\"" << jsonEscape(doc.licenseType) << "\",";
    out << "\"machineHash\":\"" << jsonEscape(doc.machineHash) << "\",";
    out << "\"orderId\":\"" << jsonEscape(doc.orderId) << "\",";
    out << "\"productId\":\"" << jsonEscape(doc.productId) << "\"";
    out << "}";
    return out.str();
}

static bool verifyEcdsaP256Signature(const std::string& payload, const std::string& signatureText) {
#ifdef _WIN32
    std::vector<unsigned char> signature;
    if (!base64Decode(signatureText, signature) || signature.size() != 64) return false;
    std::vector<unsigned char> hash = sha256Bytes(payload);

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) < 0) return false;

    BCRYPT_ECCKEY_BLOB header;
    header.dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
    header.cbKey = 32;
    std::vector<unsigned char> blob(sizeof(header) + 64, 0);
    std::memcpy(blob.data(), &header, sizeof(header));
    std::memcpy(blob.data() + sizeof(header), DUXUN_LICENSE_PUBLIC_KEY_V1_X, 32);
    std::memcpy(blob.data() + sizeof(header) + 32, DUXUN_LICENSE_PUBLIC_KEY_V1_Y, 32);

    bool ok = false;
    if (BCryptImportKeyPair(alg, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key, blob.data(), (ULONG)blob.size(), 0) >= 0) {
        ok = BCryptVerifySignature(key, nullptr, hash.data(), (ULONG)hash.size(),
                                   signature.data(), (ULONG)signature.size(), 0) >= 0;
        BCryptDestroyKey(key);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
#else
    (void)payload;
    (void)signatureText;
    return false;
#endif
}

static std::string readRegistryStringMachineGuid() {
#ifdef _WIN32
    char buffer[256];
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    LONG result = RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", "MachineGuid",
                               RRF_RT_REG_SZ, &type, buffer, &size);
    if (result == ERROR_SUCCESS && size > 1) return std::string(buffer);
#endif
    return std::string();
}

static std::string systemVolumeSerial() {
#ifdef _WIN32
    char windowsDir[MAX_PATH];
    if (!GetWindowsDirectoryA(windowsDir, MAX_PATH)) return std::string();
    char root[] = "C:\\";
    root[0] = windowsDir[0] ? windowsDir[0] : 'C';
    DWORD serial = 0;
    if (!GetVolumeInformationA(root, nullptr, 0, &serial, nullptr, nullptr, nullptr, 0)) return std::string();
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%08X", (unsigned int)serial);
    return std::string(buffer);
#else
    return std::string();
#endif
}

static std::string canonicalMachineFingerprint() {
    std::ostringstream out;
    out << "machine-v1";
    out << "|guid=" << trimLower(readRegistryStringMachineGuid());
    out << "|volume=" << trimLower(systemVolumeSerial());
    out << "|arch=" << trimLower(getEnvString("PROCESSOR_ARCHITECTURE"));
    out << "|cpu=" << trimLower(getEnvString("PROCESSOR_IDENTIFIER"));
    return out.str();
}

static std::string currentMachineHash() {
    return sha256HexUpper(std::string(kProductId) + "|" + canonicalMachineFingerprint());
}

static bool hasFeature(const LicenseDocument& doc, const char* feature) {
    for (size_t i = 0; i < doc.features.size(); ++i) {
        if (doc.features[i] == feature) return true;
    }
    return false;
}

static LocalLicenseState makeState(bool signedOk, bool trialActive, bool watermark,
                                   const char* code, const std::string& status, const std::string& detail = std::string()) {
    LocalLicenseState state;
    state.signedLicenseValid = signedOk;
    state.trialActive = trialActive;
    state.watermarkRequired = watermark;
    state.machineMismatch = std::string(code) == "machine_mismatch";
    state.expired = std::string(code) == "expired" || std::string(code) == "trial_expired";
    state.code = code;
    state.statusText = status;
    state.detail = detail;
    return state;
}

static bool trialGuardMatches(const std::string& machineHash, const std::string& startedAt,
                              const std::string& lastSeenAt, const std::string& guard) {
    std::string payload = std::string("trial-state-v1|") + kProductId + "|" + machineHash + "|" + startedAt + "|" + lastSeenAt;
    return sha256HexUpper(payload) == guard;
}

static std::string trialGuard(const std::string& machineHash, const std::string& startedAt, const std::string& lastSeenAt) {
    std::string payload = std::string("trial-state-v1|") + kProductId + "|" + machineHash + "|" + startedAt + "|" + lastSeenAt;
    return sha256HexUpper(payload);
}

static bool writeTrialState(const std::string& machineHash, const std::string& startedAt, const std::string& lastSeenAt) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"productId\": \"" << kProductId << "\",\n";
    out << "  \"trialStartedAt\": \"" << startedAt << "\",\n";
    out << "  \"lastSeenAt\": \"" << lastSeenAt << "\",\n";
    out << "  \"machineHash\": \"" << machineHash << "\",\n";
    out << "  \"localGuard\": \"" << trialGuard(machineHash, startedAt, lastSeenAt) << "\"\n";
    out << "}\n";
    return writeTextFile(trialPath(), out.str());
}

static LocalLicenseState evaluateTrial(std::time_t now, const std::string& machineHash) {
    std::string json;
    if (!readTextFile(trialPath(), json)) {
        std::string nowIso = isoUtcFromUnix(now);
        if (!writeTrialState(machineHash, nowIso, nowIso)) {
            return makeState(false, false, true, "trial_state_failed", "License: trial state could not be written");
        }
        return makeState(false, true, true, "trial_active", "License: 24h trial active, watermark enabled");
    }

    std::string productId, startedAt, lastSeenAt, fileMachineHash, guard;
    if (!extractString(json, "productId", productId) ||
        !extractString(json, "trialStartedAt", startedAt) ||
        !extractString(json, "lastSeenAt", lastSeenAt) ||
        !extractString(json, "machineHash", fileMachineHash) ||
        !extractString(json, "localGuard", guard)) {
        return makeState(false, false, true, "trial_invalid", "License: trial state invalid");
    }
    if (productId != kProductId || fileMachineHash != machineHash) {
        return makeState(false, false, true, "trial_machine_mismatch", "License: trial machine mismatch");
    }
    if (!trialGuardMatches(fileMachineHash, startedAt, lastSeenAt, guard)) {
        return makeState(false, false, true, "trial_tampered", "License: trial state was modified");
    }
    std::time_t started = 0, lastSeen = 0;
    if (!unixFromIsoUtc(startedAt, started) || !unixFromIsoUtc(lastSeenAt, lastSeen)) {
        return makeState(false, false, true, "trial_invalid_time", "License: trial time invalid");
    }
    if (now + kClockRollbackGraceSeconds < lastSeen) {
        return makeState(false, false, true, "clock_rollback", "License: system clock rollback detected");
    }
    if (now - started > kTrialSeconds) {
        return makeState(false, false, true, "trial_expired", "License: trial expired, watermark enabled");
    }
    writeTrialState(machineHash, startedAt, isoUtcFromUnix(now));
    int secondsLeft = (int)(kTrialSeconds - (now - started));
    int hoursLeft = (secondsLeft + 3599) / 3600;
    std::ostringstream status;
    status << "License: 24h trial active, " << hoursLeft << "h left, watermark enabled";
    return makeState(false, true, true, "trial_active", status.str());
}

static LocalLicenseState evaluateNow(bool forceRefresh = false) {
    static LocalLicenseState cached = makeState(false, false, true, "uninitialized", "License: checking");
    static std::time_t cachedAt = 0;
    static std::string lastLoggedCode;
    static std::mutex cacheMutex;
    std::lock_guard<std::mutex> lock(cacheMutex);

    std::time_t now = std::time(nullptr);
    if (!forceRefresh && cachedAt != 0 && now - cachedAt < 15) return cached;

    ensureLayout();
    std::string machineHash = currentMachineHash();
    std::string json;
    LocalLicenseState state;

    if (readTextFile(licensePath(), json)) {
        LicenseDocument doc;
        if (!parseLicenseJson(json, doc)) {
            state = makeState(false, false, true, "invalid_schema", "License: invalid license schema");
        } else if (doc.productId != kProductId) {
            state = makeState(false, false, true, "product_mismatch", "License: product mismatch");
        } else if (doc.keyVersion != kKeyVersion) {
            state = makeState(false, false, true, "key_mismatch", "License: unsupported keyVersion");
        } else if (!verifyEcdsaP256Signature(canonicalLicensePayload(doc), doc.signature)) {
            state = makeState(false, false, true, "invalid_signature", "License: invalid signature");
        } else if (doc.machineHash != machineHash) {
            state = makeState(false, false, true, "machine_mismatch", "License: machine mismatch");
        } else if (!hasFeature(doc, "core")) {
            state = makeState(false, false, true, "feature_missing", "License: core feature missing");
        } else if (doc.licenseType == "buyout") {
            state = makeState(true, false, false, "buyout", "License: buyout activated");
        } else if (doc.licenseType == "activated" || doc.licenseType == "trial") {
            std::time_t expires = 0;
            if (doc.expiresAtNull || !unixFromIsoUtc(doc.expiresAt, expires)) {
                state = makeState(false, false, true, "invalid_expiry", "License: invalid expiresAt");
            } else if (now > expires) {
                state = makeState(false, false, true, "expired", "License: expired");
            } else if (doc.licenseType == "trial") {
                state = makeState(true, true, true, "signed_trial", "License: signed trial active, watermark enabled");
            } else {
                state = makeState(true, false, false, "activated", "License: activated");
            }
        } else {
            state = makeState(false, false, true, "invalid_type", "License: unsupported licenseType");
        }
    } else {
        state = evaluateTrial(now, machineHash);
    }

    cached = state;
    cachedAt = now;
    if (lastLoggedCode != state.code) {
        std::ostringstream log;
        log << isoUtcFromUnix(now) << " status=" << state.code << " message=\"" << state.statusText << "\"\n";
        std::ofstream output(licenseLogPath().c_str(), std::ios::out | std::ios::app | std::ios::binary);
        if (output) output << log.str();
        lastLoggedCode = state.code;
    }
    return cached;
}

static LocalLicenseState evaluateLocalLicense() {
    return evaluateNow(false);
}

static LocalLicenseState reloadLocalLicense() {
    return evaluateNow(true);
}

static std::string writeActivationRequest() {
    std::time_t now = std::time(nullptr);
    std::string createdAt = isoUtcFromUnix(now);
    std::string machineHash = currentMachineHash();
    std::string requestId = sha256HexUpper(std::string(kProductId) + "|" + machineHash + "|" + createdAt).substr(0, 24);
    std::ostringstream out;
    out << "{\n";
    out << "  \"createdAt\": \"" << createdAt << "\",\n";
    out << "  \"machineFingerprintVersion\": \"machine-v1\",\n";
    out << "  \"machineHash\": \"" << machineHash << "\",\n";
    out << "  \"productId\": \"" << kProductId << "\",\n";
    out << "  \"productVersion\": \"5.0\",\n";
    out << "  \"requestId\": \"" << requestId << "\",\n";
    out << "  \"requestType\": \"offline_activation\"\n";
    out << "}\n";
    writeTextFile(activationRequestPath(), out.str());
    return activationRequestPath();
}

} // namespace duxun_license

#endif // DUXUN_FILM_LICENSE_H
