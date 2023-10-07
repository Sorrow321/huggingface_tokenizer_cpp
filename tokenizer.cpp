#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include "nlohmann/json.hpp"
#include <unicode/uchar.h>

using namespace std;
using json = nlohmann::json;


template <std::ctype_base::mask mask>
class IsNot
{
    std::locale myLocale;
    std::ctype<char> const* myCType;
public:
    IsNot( std::locale const& l = std::locale() )
        : myLocale( l )
        , myCType( &std::use_facet<std::ctype<char> >( l ) )
    {
    }
    bool operator()( char ch ) const
    {
        return ! myCType->is( mask, ch );
    }
};

typedef IsNot<std::ctype_base::space> IsNotSpace;

std::string trim( std::string const& original )
{
    std::string::const_iterator right = std::find_if(original.rbegin(), original.rend(), IsNotSpace()).base();
    std::string::const_iterator left = std::find_if(original.begin(), right, IsNotSpace() );
    return std::string( left, right );
}

std::vector<std::wstring> split(const std::wstring& input) {
    std::wstringstream stream(input);
    std::vector<std::wstring> words;
    std::wstring word;
    while (stream >> word) {
        words.push_back(word);
    }
    return words;
}

bool isPunctuation(UChar32 charCode) {
    UCharCategory category = static_cast<UCharCategory>(u_charType(charCode));

    switch (category) {
        case U_DASH_PUNCTUATION:
        case U_START_PUNCTUATION:
        case U_END_PUNCTUATION:
        case U_CONNECTOR_PUNCTUATION:
        case U_OTHER_PUNCTUATION:
        case U_INITIAL_PUNCTUATION:
        case U_FINAL_PUNCTUATION:
            return true;
        default:
            return false;
    }
}

bool _is_punctuation(UChar32 c)
{
    if((c >= 33 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 96) || (c >= 123 && c <= 126)) {
        return true;
    }
    if (isPunctuation(c)) {
        return true;
    }
    return false;
}

vector<wstring> run_split_on_func(const wstring& text)
{
    size_t i = 0;
    bool start_new_word = true;
    vector<vector<wchar_t>> output;

    while(i < text.length()) {
        wchar_t c = text[i];
        if (_is_punctuation(static_cast<UChar32>(c))) {
            vector<wchar_t> s;
            s.push_back(c);
            output.push_back(s);
            start_new_word = true;
        }else{
            if(start_new_word) {
                vector<wchar_t> empty_str;
                output.push_back(empty_str);
            }
            start_new_word = false;
            output.back().push_back(c);
        }
        i++;
    }

    vector<wstring> out_str;
    for (size_t i = 0; i < output.size(); i++) {
        wstring s(output[i].begin(), output[i].end());
        out_str.push_back(s);
    }
    return out_str;
}

std::string wstring_to_utf8(const std::wstring& wstr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    return converter.to_bytes(wstr);
}

std::wstring utf8_to_wstring(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    return converter.from_bytes(str);
}

class WordPieceTokenizer
{
private:
json jsonObj;
json vocab;
size_t max_input_chars_per_word;
wstring unk_token;

public:
WordPieceTokenizer(const string& config_path)
{
    std::ifstream file(config_path);
    file >> jsonObj;
    vocab = jsonObj["model"]["vocab"];
    max_input_chars_per_word = jsonObj["model"]["max_input_chars_per_word"];
    unk_token = utf8_to_wstring(jsonObj["model"]["unk_token"]);
}
int get_word_index(const wstring& word)
{
    string w_word = wstring_to_utf8(word);
    if(vocab.find(w_word) != vocab.end()) {
        //cout << "Found word. Id: " << vocab[word] << endl;
        return vocab[w_word];
    }else{
        //cout << "Not found" << endl;
        return -1;
    }
}


vector<size_t> tokenize_full(const wstring& input_text)
{
    vector<wstring> tokens = split(input_text);
    vector<wstring> basic_tokenized;
    for(size_t i = 0; i < tokens.size(); i++) {
        auto splitted_by_punc = run_split_on_func(tokens[i]);
        basic_tokenized.insert(basic_tokenized.end(), splitted_by_punc.begin(), splitted_by_punc.end());
    }

    vector<wstring> wordpiece_tokenized;
    for(size_t i = 0; i < basic_tokenized.size(); i++) {
        auto splitted_by_wordpiece = wordpiece_tokenize(basic_tokenized[i]);
        
        wordpiece_tokenized.insert(wordpiece_tokenized.end(), splitted_by_wordpiece.begin(), splitted_by_wordpiece.end());
    }
    vector<size_t> tokenized_ids;
    tokenized_ids.push_back(get_word_index(utf8_to_wstring("[CLS]")));
    vector<size_t> seq_ids = convert_tokens_to_ids(wordpiece_tokenized);
    tokenized_ids.insert(tokenized_ids.end(), seq_ids.begin(), seq_ids.end());
    tokenized_ids.push_back(get_word_index(utf8_to_wstring("[SEP]")));
    return tokenized_ids;
}


vector<wstring> wordpiece_tokenize(const wstring& input_text)
{

    vector<wstring> tokens = split(input_text);
    vector<wstring> output_tokens;
    for(size_t i = 0; i < tokens.size(); i++) {
        auto& tok = tokens[i];
        if(tok.length() > max_input_chars_per_word) {
            output_tokens.push_back(unk_token);
            continue;
        }

        bool is_bad = false;
        size_t start = 0;
        vector<wstring> sub_tokens;

        while(start < tok.length()) {
            size_t end = tok.length();
            wstring cur_substr;
            while(start < end) {
                wstring substr = tok.substr(start, end-start);
                if(start > 0) {
                    substr = L"##" + substr;
                }
                size_t idx = get_word_index(substr);
                if(idx != -1) {
                    cur_substr = substr;
                    break;
                }
                end--;
            }

            if(cur_substr.empty()) {
                is_bad = true;
                break;
            }
            sub_tokens.push_back(cur_substr);
            start = end;
        }

        if(is_bad) {
            output_tokens.push_back(unk_token);
        }else{
            output_tokens.insert(output_tokens.end(), sub_tokens.begin(), sub_tokens.end());
        }
    }
    return output_tokens;
}

vector<size_t> convert_tokens_to_ids(const vector<wstring>& input_seq)
{
    vector<size_t> output_ids;
    for(size_t i = 0; i < input_seq.size(); i++) {
        output_ids.push_back(get_word_index(input_seq[i]));
    }
    return output_ids;
}

};



int main()
{
    std::locale::global(std::locale(""));
    WordPieceTokenizer tokenizer("tokenizer.json");
    
    wstring input_text;
    getline(wcin, input_text);
    auto r = tokenizer.tokenize_full(input_text);
    wcout << "===== TOKENS START=====" << endl;
    for(auto &x : r) {
        wcout << x << endl;
    }
    wcout << "===== TOKENS END ======" << endl;
}