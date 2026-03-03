/*
 * Memory-Efficient Versioned File Indexer
 * CS253 Assignment 1
 *
 * Builds a word-level index over large text files using a fixed-size buffer.
 * Supports word count, difference, and top-K queries across versioned files.
 *
 * Design:
 *   - BufferedFileReader : fixed-size buffered file I/O
 *   - Tokenizer          : extracts words, handles boundary splits
 *   - WordIndex<T>       : user-defined template for word frequency storage
 *   - VersionedIndex     : manages per-version indexes
 *   - Query (abstract)   : base class with pure virtual functions
 *     - WordCountQuery   : derived – single word frequency
 *     - DiffQuery        : derived – frequency difference between versions
 *     - TopKQuery        : derived – top K frequent words
 *
 * C++ features demonstrated:
 *   - Inheritance / abstract base class + 3 derived classes
 *   - Runtime polymorphism (virtual dispatch)
 *   - Function overloading (Tokenizer::tokenize, WordIndex::addWord)
 *   - Exception handling (try / catch / throw)
 *   - User-defined class template (WordIndex<CountType>)
 */

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <cctype>
#include <cstring>
#include <memory>
#include <sstream>

using namespace std;

// ============================================================
// Template Class : WordIndex<CountType>
// Stores word -> frequency mapping. Template parameter controls
// the numeric type used for counts.
// ============================================================
template <typename CountType = long long>
class WordIndex {
private:
    unordered_map<string, CountType> index; //stores word frequencies in a unordered_map for O(1) average access

public:
    // --- Function overload 1: increment by 1 ---
    void addWord(const string& word) {
        index[word]++;
    }

    // --- Function overload 2: increment by arbitrary count ---
    void addWord(const string& word, CountType count) {
        index[word] += count;
    }

    // Returns the frequency of the word, or 0 if not found
    CountType getCount(const string& word) const {
        auto it = index.find(word);
        if (it != index.end()) return it->second;
        return 0;
    }

    // Returns a vector of the top-K most frequent words, sorted by frequency
    vector<pair<string, CountType>> getTopK(int k) const {
        vector<pair<string, CountType>> entries(index.begin(), index.end());
        // Sort descending by frequency; ties broken alphabetically
        sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) {
                if (a.second != b.second) return a.second > b.second;
                return a.first < b.first;
            });
        if (static_cast<int>(entries.size()) > k)
            entries.resize(k);
        return entries;
    }
};

// ============================================================
// BufferedFileReader
// Reads a file in fixed-size chunks so the whole file is never
// loaded into memory.
// ============================================================
class BufferedFileReader {
private:
    ifstream file;
    char* buffer;
    size_t bufferSize;
    size_t bytesRead;
    bool fileEnded;

public:
    BufferedFileReader(const string& filePath, size_t bufferSizeKB) {
        if (bufferSizeKB < 256 || bufferSizeKB > 1024) {
            throw invalid_argument(
                "Buffer size must be between 256 and 1024 KB. Given: "
                + to_string(bufferSizeKB) + " KB");
        }
        bufferSize = bufferSizeKB * 1024;
        buffer = new char[bufferSize];
        file.open(filePath, ios::binary);
        if (!file.is_open()) {
            delete[] buffer;
            throw runtime_error("Cannot open file: " + filePath);
        }
        bytesRead  = 0;
        fileEnded  = false;
    }

    ~BufferedFileReader() {
        delete[] buffer;
        if (file.is_open()) file.close();
    }

    // Non-copyable
    BufferedFileReader(const BufferedFileReader&)            = delete;
    BufferedFileReader& operator=(const BufferedFileReader&) = delete;

    // Returns false when no more data is available
    bool readNextChunk() {
        if (fileEnded) return false;
        file.read(buffer, static_cast<streamsize>(bufferSize));
        bytesRead = static_cast<size_t>(file.gcount());
        if (bytesRead == 0) {
            fileEnded = true;
            return false;
        }
        if (bytesRead < bufferSize) fileEnded = true;
        return true;
    }

    const char*  getBuffer()     const { return buffer; }
    size_t       getBytesRead()  const { return bytesRead; }
    size_t       getBufferSize() const { return bufferSize; }
    bool         isFinished()    const { return fileEnded; }
};

// ============================================================
// Tokenizer
// Extracts words (contiguous alphanumeric sequences) from raw
// buffer data. Handles tokens that span buffer boundaries by
// keeping a partial-token accumulator between calls.
// ============================================================
class Tokenizer {
private:
    string partialToken;

public:
    // --- Overloaded tokenize (1): buffer + length ---
    // isLastChunk = true flushes the partial token
    vector<string> tokenize(const char* data, size_t length, bool isLastChunk) {
        vector<string> tokens;
        string current = move(partialToken);
        partialToken.clear();

        for (size_t i = 0; i < length; ++i) {
            unsigned char c = static_cast<unsigned char>(data[i]);
            if (isalnum(c)) {
                current += static_cast<char>(tolower(c));
            } else {
                if (!current.empty()) {
                    tokens.push_back(move(current));
                    current.clear();
                }
            }
        }

        if (!current.empty()) {
            if (isLastChunk) {
                tokens.push_back(move(current));
            } else {
                partialToken = move(current);
            }
        }

        return tokens;
    }

    // --- Overloaded tokenize (2): convenience for a whole string ---
    vector<string> tokenize(const string& text) {
        return tokenize(text.c_str(), text.size(), true);
    }

    void reset() { partialToken.clear(); }
};

// ============================================================
// VersionedIndex
// Manages multiple named versions, each backed by a WordIndex.
// ============================================================
class VersionedIndex {
private:
    unordered_map<string, WordIndex<long long>> versions;

public:
    void buildIndex(const string& versionName,
                    const string& filePath,
                    size_t bufferSizeKB) {
        if (versions.count(versionName)) {
            throw runtime_error("Version already exists: " + versionName);
        }

        BufferedFileReader reader(filePath, bufferSizeKB);
        Tokenizer tokenizer;
        WordIndex<long long> idx;

        while (reader.readNextChunk()) {
            auto tokens = tokenizer.tokenize(
                reader.getBuffer(),
                reader.getBytesRead(),
                reader.isFinished());
            for (const auto& tok : tokens) {
                idx.addWord(tok);
            }
        }

        versions[versionName] = move(idx);
    }

    const WordIndex<long long>& getVersion(const string& name) const {
        auto it = versions.find(name);
        if (it == versions.end()) {
            throw runtime_error("Version not found: " + name);
        }
        return it->second;
    }

    bool hasVersion(const string& name) const {
        return versions.count(name) > 0;
    }
};

// ============================================================
// Query  (Abstract Base Class)
// Provides the interface for all query types via pure virtual
// functions, enabling runtime polymorphism through dynamic
// dispatch.
// ============================================================
class Query {
protected:
    VersionedIndex& indexer;

public:
    explicit Query(VersionedIndex& idx) : indexer(idx) {}
    virtual ~Query() = default;

    virtual void        execute()      const = 0;   // pure virtual
    virtual string getQueryType() const = 0;   // pure virtual
};

// ============================================================
// WordCountQuery  (Derived from Query)
// Returns the frequency of a single word in one version.
// ============================================================
class WordCountQuery : public Query {
private:
    string version;
    string word;

public:
    WordCountQuery(VersionedIndex& idx,
                   const string& ver,
                   const string& w)
        : Query(idx), version(ver), word(w) {}

    void execute() const override {
        string lowerWord;
        for (char c : word)
            lowerWord += static_cast<char>(tolower(static_cast<unsigned char>(c)));

        const auto& idx = indexer.getVersion(version);
        long long count = idx.getCount(lowerWord);

        cout << "Version  : " << version  << "\n";
        cout << "Word     : " << word      << "\n";
        cout << "Frequency: " << count     << "\n";
    }

    string getQueryType() const override { return "word"; }
};

// ============================================================
// DiffQuery  (Derived from Query)
// Computes the frequency difference of a word between two
// versions (version1 − version2).
// ============================================================
class DiffQuery : public Query {
private:
    string version1;
    string version2;
    string word;

public:
    DiffQuery(VersionedIndex& idx,
              const string& v1,
              const string& v2,
              const string& w)
        : Query(idx), version1(v1), version2(v2), word(w) {}

    void execute() const override {
        string lowerWord;
        for (char c : word)
            lowerWord += static_cast<char>(tolower(static_cast<unsigned char>(c)));

        const auto& idx1 = indexer.getVersion(version1);
        const auto& idx2 = indexer.getVersion(version2);

        long long count1 = idx1.getCount(lowerWord);
        long long count2 = idx2.getCount(lowerWord);
        long long diff   = count1 - count2;

        cout << "Version 1 : " << version1 << "\n";
        cout << "Version 2 : " << version2 << "\n";
        cout << "Word      : " << word      << "\n";
        cout << "Frequency in " << version1 << ": " << count1 << "\n";
        cout << "Frequency in " << version2 << ": " << count2 << "\n";
        cout << "Difference (" << version1 << " - " << version2 << "): "
                  << diff << "\n";
    }

    string getQueryType() const override { return "diff"; }
};

// ============================================================
// TopKQuery  (Derived from Query)
// Displays the top-K most frequent words in a version, sorted
// in descending order of frequency.
// ============================================================
class TopKQuery : public Query {
private:
    string version;
    int k;

public:
    TopKQuery(VersionedIndex& idx,
              const string& ver,
              int topK)
        : Query(idx), version(ver), k(topK) {}

    void execute() const override {
        if (k <= 0) {
            throw invalid_argument("Top-K value must be a positive integer");
        }

        const auto& idx = indexer.getVersion(version);
        auto topWords = idx.getTopK(k);

        cout << "Version: " << version << "\n";
        cout << "Top " << k << " words:\n";
        for (int i = 0; i < static_cast<int>(topWords.size()); ++i) {
            cout << "  " << (i + 1) << ". " << topWords[i].first
                      << " : " << topWords[i].second << "\n";
        }
    }

    string getQueryType() const override { return "top"; }
};

// ============================================================
// Main – CLI parsing, index building, query dispatch
// ============================================================
int main(int argc, char* argv[]) {
    try {
        auto startTime = chrono::high_resolution_clock::now();

        // --- Parse command-line arguments ---
        string file, file1, file2;
        string version, version1, version2;
        int         bufferKB  = 0;
        string queryType;
        string word;
        int         topK      = 0;

        for (int i = 1; i < argc; ++i) {
            string arg = argv[i];

            if      (arg == "--file"     && i + 1 < argc) file     = argv[++i];
            else if (arg == "--file1"    && i + 1 < argc) file1    = argv[++i];
            else if (arg == "--file2"    && i + 1 < argc) file2    = argv[++i];
            else if (arg == "--version"  && i + 1 < argc) version  = argv[++i];
            else if (arg == "--version1" && i + 1 < argc) version1 = argv[++i];
            else if (arg == "--version2" && i + 1 < argc) version2 = argv[++i];
            else if (arg == "--buffer"   && i + 1 < argc) bufferKB = stoi(argv[++i]);
            else if (arg == "--query"    && i + 1 < argc) queryType= argv[++i];
            else if (arg == "--word"     && i + 1 < argc) word     = argv[++i];
            else if (arg == "--top"      && i + 1 < argc) topK     = stoi(argv[++i]);
            else {
                throw invalid_argument("Unknown or incomplete argument: " + arg);
            }
        }

        // --- Validate common inputs ---
        if (bufferKB < 256 || bufferKB > 1024) {
            throw invalid_argument(
                "Buffer size must be between 256 and 1024 KB. Given: "
                + to_string(bufferKB) + " KB");
        }

        if (queryType != "word" && queryType != "diff" && queryType != "top") {
            throw invalid_argument(
                "Query type must be 'word', 'diff', or 'top'. Given: " + queryType);
        }

        // --- Build indexes & create query (polymorphic) ---
        VersionedIndex indexer;
        unique_ptr<Query> query;

        if (queryType == "word") {
            if (file.empty() || version.empty() || word.empty()) {
                throw invalid_argument(
                    "Word query requires --file, --version, and --word");
            }
            indexer.buildIndex(version, file, bufferKB);
            query = make_unique<WordCountQuery>(indexer, version, word);

        } else if (queryType == "top") {
            if (file.empty() || version.empty() || topK <= 0) {
                throw invalid_argument(
                    "Top query requires --file, --version, and --top (positive integer)");
            }
            indexer.buildIndex(version, file, bufferKB);
            query = make_unique<TopKQuery>(indexer, version, topK);

        } else if (queryType == "diff") {
            if (file1.empty() || file2.empty() ||
                version1.empty() || version2.empty() || word.empty()) {
                throw invalid_argument(
                    "Diff query requires --file1, --file2, --version1, --version2, and --word");
            }
            indexer.buildIndex(version1, file1, bufferKB);
            indexer.buildIndex(version2, file2, bufferKB);
            query = make_unique<DiffQuery>(indexer, version1, version2, word);
        }

        // --- Execute via dynamic dispatch ---
        cout << "===== Versioned File Indexer =====\n";
        cout << "Query Type  : " << query->getQueryType() << "\n";
        query->execute();
        cout << "Buffer Size : " << bufferKB << " KB\n";

        auto endTime = chrono::high_resolution_clock::now();
        chrono::duration<double> elapsed = endTime - startTime;
        cout << "Execution Time: " << elapsed.count() << " seconds\n";

    } catch (const invalid_argument& e) {
        cerr << "Invalid argument: " << e.what() << endl;
        return 1;
    } catch (const runtime_error& e) {
        cerr << "Runtime error: " << e.what() << endl;
        return 1;
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
