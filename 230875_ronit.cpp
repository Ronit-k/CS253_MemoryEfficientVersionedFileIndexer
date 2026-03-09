/*
 * Memory-Efficient Versioned File Indexer
 * CS253 Assignment 1
 * Name: Ronit Kumar
 * Roll No: 230875
 * Mail ID: ronitk23@iitk.ac.in
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
//####################################################################
// PROGRESS BAR INCLUDE — only for visual progress display.        //#
// Has nothing to do with the core logic of the program.           //#
// Can be safely deleted without affecting functionality.          //#
//####################################################################
// Conditionally include the progress bar header.                  //################
// If progressbar.h is not present, everything compiles and runs fine without it. ###
#if __has_include("progressbar.h")                                 //################
    #include "progressbar.h"                                       //#
    #define HAS_PROGRESSBAR 1                                      //#
#else                                                              //#
    #define HAS_PROGRESSBAR 0                                      //#
#endif                                                             //#
//####################################################################




using namespace std;

// ============================================================
// Template Class : WordIndex<CountType>
// Stores word -> frequency mapping. Template parameter controls
// the numeric type used for counts.
// ============================================================
template <typename CountType = long long> //default to long long for large counts, but can be customized
class WordIndex {
private:
    unordered_map<string, CountType> frequencyMap; //stores word frequencies in a unordered_map for O(1) average access

public:
    // Adds a word to the index, incrementing its frequency by 1
    void addWord(const string& word) {
        frequencyMap[word]++;
    }

    // Returns the frequency of the word, or 0 if not found
    CountType getCount(const string& word) const {
        auto it = frequencyMap.find(word);
        if (it != frequencyMap.end()) return it->second;
        return 0;
    }

    // Returns a vector of the top-K most frequent words, sorted by frequency
    vector<pair<string, CountType>> getTopK(int k) const {
        vector<pair<string, CountType>> entries(frequencyMap.begin(), frequencyMap.end());
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
    size_t chunkBytesRead;
    bool fileEnded;

public:
    // Constructor definition
    BufferedFileReader(const string& filePath, size_t bufferSizeKB) {
        // Exception handling for invalid buffer sizes
        if (bufferSizeKB < 256 || bufferSizeKB > 1024) {
            throw invalid_argument(
                "Buffer size must be between 256 and 1024 KB. Given: "
                + to_string(bufferSizeKB) + " KB");
        }
        bufferSize = bufferSizeKB * 1024; // Convert KB to bytes
        buffer = new char[bufferSize];    // Allocate buffer of specified size
        file.open(filePath, ios::binary); // Open file in binary mode

        // Exception handling for file opening failure
        if (!file.is_open()) {
            delete[] buffer;
            throw runtime_error("Cannot open file: " + filePath);
        }
        chunkBytesRead = 0;
        fileEnded      = false;
    }
    // Destructor definition
    ~BufferedFileReader() {
        delete[] buffer;
        if (file.is_open()) file.close();
    }

    // Prevent copying: this class owns a raw pointer (buffer) and a file handle,
    // so a shallow copy would cause double-free and duplicate file access.
    BufferedFileReader(const BufferedFileReader&)            = delete;
    BufferedFileReader& operator=(const BufferedFileReader&) = delete;

    // Loads the next chunk of raw bytes from the file into the buffer.
    // Returns true if data was loaded, false if the file has been fully read and no more data was loaded.
    // After a successful call, use getBuffer() and getBytesRead() to access the data.
    bool loadNextChunk() {
        if (fileEnded) return false;
    file.read(buffer, static_cast<streamsize>(bufferSize)); // Read next chunk into buffer - MAIN OPERATION THAT KEEPS MEMORY USAGE ~CONSTANT 
        chunkBytesRead = static_cast<size_t>(file.gcount());
        if (chunkBytesRead == 0) { // no data loaded => end of file reached and nothing to read
            fileEnded = true;
            return false;
        }
        if (chunkBytesRead < bufferSize) fileEnded = true; // less than buffer size data loaded => end of file reached
        return true;
    }

    // Accessors for buffer data and status
    const char*  getBuffer()         const { return buffer; }
    size_t       getChunkBytesRead() const { return chunkBytesRead; }
    bool         isFinished()        const { return fileEnded; }
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
    // --- tokenize Function overload 1: buffer + length ---
    vector<string> tokenize(const char* data, size_t length, bool isLastChunk) {
        vector<string> tokens;
        string current = move(partialToken);
        partialToken.clear();

        for (size_t i = 0; i < length; ++i) {
            unsigned char c = static_cast<unsigned char>(data[i]);

            if (isalnum(c)) { // Alphanumeric character continues the current word
                current += static_cast<char>(tolower(c));
            } 
            else { // Non-alphanumeric character ends the current word
                if (!current.empty()) {
                    tokens.push_back(move(current));
                    current.clear();
                }
            }
        }

        // If we ended with a partial word and this is not the last chunk, save it for the next call
        if (!current.empty()) {
            if (isLastChunk) {
                tokens.push_back(move(current));
            } else {
                partialToken = move(current);
            }
        }

        return tokens;
    }

    // --- tokenize Function overload 2: A convenience wrapper. Calls overload 1 with: (pointer to the string's underlying char array, its length, true) ---
    vector<string> tokenize(const string& text) {
        return tokenize(text.c_str(), text.size(), true);
    }
};

// ============================================================
// VersionedIndex
// Manages multiple named versions, each backed by a WordIndex.
// ============================================================
class VersionedIndex {
private:
    unordered_map<string, WordIndex<long long>> versions;

public:
    void buildIndex(const string& versionName, const string& filePath, size_t bufferSizeKB) {
        if (versions.count(versionName)) {
            throw runtime_error("Version already exists: " + versionName);
        }

        BufferedFileReader reader(filePath, bufferSizeKB);
        Tokenizer tokenizer;
        WordIndex<long long> wordIndex;

//####################################################################
// PROGRESS BAR SETUP — only for visual progress display.          //#
// Has nothing to do with the core logic of the program.           //#
// Can be safely deleted without affecting functionality.          //#
//####################################################################
#if HAS_PROGRESSBAR                                                //#
        // Determine file size for the progress bar                //#
        size_t fileSize = 0;                                       //#
        {                                                          //#
            ifstream sizeCheck(filePath, ios::binary | ios::ate);  //#
            if (sizeCheck.is_open())                               //#
                fileSize = static_cast<size_t>(sizeCheck.tellg()); //#
        }                                                          //#
        ProgressBar progressBar(fileSize,"Indexing "+versionName); //#
#endif                                                             //# 
//####################################################################

        while (reader.loadNextChunk()) {
//####################################################################
// PROGRESS BAR UPDATE — only for visual progress display.         //#
// Has nothing to do with the core logic of the program.           //#
// Can be safely deleted without affecting functionality.          //#    
#if HAS_PROGRESSBAR                                                //#
            progressBar.update(reader.getChunkBytesRead());        //#
#endif                                                             //#
//####################################################################
            auto tokens = tokenizer.tokenize(
                reader.getBuffer(),                 // pointer to buffer data
                reader.getChunkBytesRead(),         // number of bytes loaded into the buffer
                reader.isFinished());               // whether this is the last chunk (end of file reached)

            // Add all words/tokens from the list of words/tokens returned by the tokenizer
            for (const auto& tok : tokens) {
                wordIndex.addWord(tok);
            }
        }

//####################################################################
// PROGRESS BAR FINISH — only for visual progress display.         //# 
// Has nothing to do with the core logic of the program.           //#
// Can be safely deleted without affecting functionality.          //#
#if HAS_PROGRESSBAR                                                //#
        progressBar.finish();                                      //#
#endif                                                             //# 
//####################################################################

        versions[versionName] = move(wordIndex);
    }

    // Returns a const reference to the WordIndex for the given version name.
    const WordIndex<long long>& getWordIndex(const string& versionName) const {
        auto it = versions.find(versionName);
        if (it == versions.end()) {
            throw runtime_error("Version not found: " + versionName);
        }
        return it->second;
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
    VersionedIndex& versionedIndex; 

public:
    // Constructor that initializes the reference to the VersionedIndex and validates the query type
    Query(VersionedIndex& vIdx, const string& queryType) : versionedIndex(vIdx) {
        if (queryType != "word" && queryType != "diff" && queryType != "top") {
            throw invalid_argument(
                "Query type must be 'word', 'diff', or 'top'. Given: " + queryType);
        }
    }
    // Virtual destructor
    virtual ~Query() = default;

    // Pure virtual functions to execute the query. Must be overridden by derived classes.
    virtual void        execute() const = 0;   
    virtual string getQueryType() const = 0;
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
    WordCountQuery(VersionedIndex& vIdx, const string& ver, const string& w)
                    : Query(vIdx, "word"), version(ver), word(w) {}

    // Polymorphic execute() function that overrides the pure virtual function in the base class. Executes the word count query and prints the results.
    void execute() const override {
        // get the frequency count of the word in the specified version
        long long count = versionedIndex.getWordIndex(version).getCount(word);

        cout << "Version: " << version << "\n";
        cout << "Count: " << count << "\n";
    }

    // Polymorphic getQueryType() function that overrides the pure virtual function in the base class. Returns the string "word" to indicate this is a word count query.
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
    DiffQuery(VersionedIndex& vIdx,const string& v1,const string& v2,const string& w)
                : Query(vIdx, "diff"), version1(v1), version2(v2), word(w) {}

    // Polymorphic execute() function that overrides the pure virtual function in the base class. Executes the difference query and prints the results.
    void execute() const override {
        long long count1 = versionedIndex.getWordIndex(version1).getCount(word);
        long long count2 = versionedIndex.getWordIndex(version2).getCount(word);
        long long diff   = count2 - count1;

        cout << "Difference (" << version2 << " - " << version1 << "): "
                  << diff << "\n";
    }
    // Polymorphic getQueryType() function that overrides the pure virtual function in the base class. Returns the string "diff" to indicate this is a difference query.
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
    TopKQuery(VersionedIndex& vIdx,const string& ver,int topK)
                : Query(vIdx, "top"), version(ver), k(topK) {}

    // Polymorphic execute() function that overrides the pure virtual function in the base class. Executes the top-K query and prints the results.
    void execute() const override {
        if (k <= 0) {
            throw invalid_argument("Top-K value must be a positive integer");
        }

        auto topWords = versionedIndex.getWordIndex(version).getTopK(k);

        cout << "Top-" << k << " words in version " << version << ":\n";
        for (int i = 0; i < static_cast<int>(topWords.size()); i++) {
            cout << topWords[i].first << " " << topWords[i].second << "\n";
        }
    }
    // Polymorphic getQueryType() function that overrides the pure virtual function in the base class. Returns the string "top" to indicate this is a top-K query.
    string getQueryType() const override { return "top"; }
};

// ============================================================
// Helper – prompt for a missing string argument interactively
// ============================================================
static string promptIfEmpty(const string& value, const string& prompt) {
    if (!value.empty()) return value;
    string input;
    cout << prompt;
    getline(cin, input);
    // trim leading/trailing whitespace
    size_t s = input.find_first_not_of(" \t\r\n");
    size_t e = input.find_last_not_of(" \t\r\n");
    if (s == string::npos) return "";
    return input.substr(s, e - s + 1);
}

// Helper – prompt for a missing integer argument interactively
static int promptIntIfZero(int value, const string& prompt) {
    if (value > 0) return value;
    string input;
    cout << prompt;
    getline(cin, input);
    return stoi(input);
}

// ============================================================
// Main – CLI parsing, interactive prompts, index building,
//        query dispatch
// ============================================================
int main(int argc, char* argv[]) {
    try {

        // --- Parse command-line arguments ---
        string file, file1, file2;
        string version, version1, version2;
        int bufferKB = 0;
        string queryType;
        string word;
        int topK = 0;

        for (int i = 1; i < argc; i++) {
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

        // --- Interactively prompt for any missing arguments ---

        // Query type is asked first because it determines which other args are needed
        queryType = promptIfEmpty(queryType, "Enter query type (word / diff / top): ");
        if (queryType != "word" && queryType != "diff" && queryType != "top") {
            throw invalid_argument(
                "Query type must be 'word', 'diff', or 'top'. Given: " + queryType);
        }

        // Buffer size is needed for every query
        bufferKB = promptIntIfZero(bufferKB, "Enter buffer size in KB (256–1024): ");

        // Prompt for query-specific arguments
        if (queryType == "word" || queryType == "top") {
            file    = promptIfEmpty(file,    "Enter file path (--file): ");
            version = promptIfEmpty(version, "Enter version name (--version): ");

            if (queryType == "word") {
                word = promptIfEmpty(word, "Enter word to search (--word): ");
            } 
            else {  // top
                topK = promptIntIfZero(topK, "Enter top-K value (--top): ");
            }

        } 
        else {  // diff
            file1    = promptIfEmpty(file1,    "Enter first file path (--file1): ");
            version1 = promptIfEmpty(version1, "Enter first version name (--version1): ");
            file2    = promptIfEmpty(file2,    "Enter second file path (--file2): ");
            version2 = promptIfEmpty(version2, "Enter second version name (--version2): ");
            word     = promptIfEmpty(word,     "Enter word to compare (--word): ");
        }

        // Now we have all the arguments we need, even if some were missing from the command line prompt, we interactively asked the user for them.
        // Start timing AFTER all arguments are collected (so interactive prompts don't inflate the time)
        auto startTime = chrono::high_resolution_clock::now();

        // Normalize the query word using the tokenizer's string overload (handles lowercasing + cleanup)
        if (!word.empty()) {
            Tokenizer tokenizer;
            auto tokens = tokenizer.tokenize(word);  // tokenize overload 2: string → lowercase tokens
            word = tokens.empty() ? "" : tokens[0];
        }

        // --- Validate & build indexes, create query (polymorphic via base class pointer) ---
        VersionedIndex indexer;
        Query* query = nullptr;

        if (queryType == "word") {
            if (file.empty() || version.empty() || word.empty()) {
                throw invalid_argument(
                    "Word query requires --file, --version, and --word");
            }
            indexer.buildIndex(version, file, bufferKB);
            query = new WordCountQuery(indexer, version, word);

        } else if (queryType == "top") {
            if (file.empty() || version.empty() || topK <= 0) {
                throw invalid_argument(
                    "Top query requires --file, --version, and --top (positive integer)");
            }
            indexer.buildIndex(version, file, bufferKB);
            query = new TopKQuery(indexer, version, topK);

        } else if (queryType == "diff") {
            if (file1.empty() || file2.empty() ||
                version1.empty() || version2.empty() || word.empty()) {
                throw invalid_argument(
                    "Diff query requires --file1, --file2, --version1, --version2, and --word");
            }
            indexer.buildIndex(version1, file1, bufferKB);
            indexer.buildIndex(version2, file2, bufferKB);
            query = new DiffQuery(indexer, version1, version2, word);
        }

        // --- Execute via dynamic dispatch ---
        query->execute();
        cout << "Buffer Size (KB): " << bufferKB << "\n";

        auto endTime = chrono::high_resolution_clock::now();
        chrono::duration<double> elapsed = endTime - startTime;
        cout << "Execution Time (s): " << elapsed.count() << "\n";

        delete query;  // Manual cleanup — must not forget this

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
