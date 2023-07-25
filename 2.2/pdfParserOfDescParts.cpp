#include <iostream>
#include <vector>
#include <string>
#include <codecvt>
#include <locale>
#include <poppler-document.h>
#include <poppler/cpp/poppler-page.h>

using std::vector;
using std::string;

class PDF_Parser {
public:
    PDF_Parser(poppler::document* doc) : doc(doc) {}
    vector<string> find_description_parts();
    int get_desc_num() { return desc_page_num; }
private:
    int find_description_page_num();
    poppler::document* doc;
    int desc_page_num = -1;
};

int PDF_Parser::find_description_page_num() {
    // Получаем количество страниц в документе
    int numPages = doc->pages();

    // Переменная для хранения номера страницы описания
    int descriptionPage = -1;

    for (int i = 0; i < numPages; i++) {
        // Получаем страницу по индексу
        poppler::page* page = doc->create_page(i);

        // Получаем текст на странице
        poppler::byte_array textBytes = page->text().to_utf8();
        std::string text(textBytes.begin(), textBytes.end());

        // Проверяем, содержит ли текст страницы описание, на этой странице указан книжный номер
        if (text.find("ISBN") != std::string::npos) {
            descriptionPage = i + 1; // Страницы в PDF нумеруются с 1
            break;
        }

        // Освобождаем ресурсы страницы
        delete page;
    }

    this->desc_page_num = descriptionPage;

    return descriptionPage;
}

bool get_pre_info_str(string text, int& start, int& end) {
    while (end > 0 && text[end] != '\n') end--;
    if (end > 0) {
        end--;
    } else return false;
    start = end;
    while (start > 0 && !(text[start] == '\n' && text[start - 1] == '\n')) start--;
    start++;

    return start >= 0;
}

bool get_annotation_str(string text, int& start, int& end) {
    while (start < text.size() && !(text[start] != '\n' && text[start - 1] == '\n')) start++;
    if (start + 1 < text.size()) {
        start++;
    } else return false;
    
    end = start;
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    std::u16string utf16text = converter.from_bytes(text);
    int endPosFirst = text.find("УДК", start);
    int endPosSec = text.find("ББК", start);
    end = std::min(endPosFirst, endPosSec);
    while (end >= 0 && text[end] != '.') end--;

    // while (end + 2 < text.size()) {
    //     if (
    //         utf16text[end] == u'У' && utf16text[end + 1] == u'Д' && utf16text[end + 2] == u'К'
    //         || utf16text[end] == u'Б' && utf16text[end + 1] == u'Б' && utf16text[end + 2] == u'К'
    //         ) 
    //     {
    //         // std::cout << text[end] << text[end + 1] << text[end + 2];
    //         // while (end >= 0 && text[end] != '.') end--;
    //         if (end < 0) return false;

    //         return true;
    //     }

    //     end++;
    // }

    return start >= 0;
}

vector<string> split_tokens(string str) {
    int start = 0, end = 0;
    vector<string> result;
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    std::u16string utf16str = converter.from_bytes(str);
    while (end < utf16str.size()) {
        if (utf16str[end] == u'–' || utf16str[end] == u'—') {
            while (isspace(utf16str[start])) start++;
            result.push_back(converter.to_bytes(utf16str.substr(start, end - start)));
            start = ++end;
        } else {
            end++;
        }
    }

    if (start != end) {
        result.push_back(converter.to_bytes(utf16str.substr(start, end - start)));
    }

    return result;
}

string formatStr(string str) {
    int ready = 0, current = 0;
    while (current < str.size() && isspace(str[current])) current++;
    while (current < str.size()) {
        if (current + 1 < str.size() && str[current] == '-' && str[current + 1] == '\n') {
            current += 2;
            while (current < str.size() && isspace(str[current])) current++;
        } else if (str[current] == '\n') {
            current++;
            while (current + 1 < str.size() && isspace(str[current + 1])) current++;
        } else {
            str[ready++] = str[current++];
        }
    }

    str.resize(ready);
    return str;
}

// Получаем пару издательство год 
std::pair<string, int> getPublishAndYear(string str) {
    int start = 0, end = 0, endOfPublish = 0;
    std::pair<string, int> result;
    while (isspace(str[start])) start++;
    while (end + 3 < str.size()) {
        if (isdigit(str[end]) && isdigit(str[end + 1]) && isdigit(str[end + 2]) && isdigit(str[end + 3])) {
            endOfPublish = end;
            while (endOfPublish >= 0 && str[endOfPublish + 1] != ',') endOfPublish--;
           break;
        }

        end++;
    }

    result.first = str.substr(start, endOfPublish - start + 1);
    result.second = stoi(str.substr(end, 4));

    return result;
}

vector<string> PDF_Parser::find_description_parts() {
    // {title, author, year, publish, annotation}
    vector<string> result_data;
    result_data.assign(5, "");
    this->find_description_page_num();
    if (desc_page_num == -1) {
        return {};
    }

    // Получаем текст страницы с описанием
    poppler::page* page = doc->create_page(desc_page_num - 1);
    poppler::byte_array textBytes = page->text().to_utf8();
    std::string text(textBytes.begin(), textBytes.end());
    // std::cout << text;

    int middleInd = 0;
    middleInd = text.find("ISBN");
    int preStartInd = middleInd, preEndInd = middleInd, annotationStartInd = middleInd, annotationEndInd = middleInd;
    get_pre_info_str(text, preStartInd, preEndInd);
    get_annotation_str(text, annotationStartInd, annotationEndInd);

    result_data[4] = formatStr(text.substr(annotationStartInd, annotationEndInd - annotationStartInd + 1));
    int authorEnd = preStartInd;
    while (authorEnd + 1 < text.size() && text[authorEnd + 1] != '\n') authorEnd++;
    result_data[1] = formatStr(text.substr(preStartInd, authorEnd - preStartInd + 1));

    int startOfTitle = authorEnd;
    while (startOfTitle - 2 < text.size() && text[startOfTitle - 1] != '\t' && !(text[startOfTitle - 1] == ' ' && text[startOfTitle - 2] == ' ')) startOfTitle++;
    vector<string> descTokens = split_tokens(text.substr(startOfTitle, preEndInd - startOfTitle + 1));
    result_data[0] = formatStr(descTokens[0]);

    std::pair<string, int> publishPair = getPublishAndYear(descTokens[1]);
    result_data[2] = std::to_string(publishPair.second);
    result_data[3] = formatStr(publishPair.first);

    // Освобождаем ресурсы страницы
    delete page;

    return result_data;
}

int main() {
    setlocale(LC_CTYPE, ".1251");
    setlocale(LC_ALL, "Russian");
    string filename = "somepdf.pdf";
    string filename2 = "Dauni_A._Osnovy_Python.pdf";
    string filename3 = "CHistaya_arhitektura_Iskusstvo_razrabotki.pdf";

    // Открываем PDF-документ
    poppler::document* doc = poppler::document::load_from_file("../" + filename2);
    PDF_Parser parserObj(doc);

    if (doc != nullptr) {
        vector<string> data = parserObj.find_description_parts();

        // Освобождаем ресурсы документа
        delete doc;

        // Выводим результат
        if (data.size() != 0) {
            for (int i = 0; i < data.size(); ++i) {
                std::cout << data[i];
                if (i != data.size() - 1) {
                    std::cout << '|';
                }
            }

            std::cout << std::endl;
        } else {
            std::cout << "Данные не найдены." << std::endl;
        }
    } else {
        std::cout << "Не удалось открыть PDF-документ." << std::endl;
    }

    return 0;
}
