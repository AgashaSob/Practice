#include <iostream>
#include <vector>
#include <string>
#include <codecvt>
#include <locale>
#include <poppler-document.h>
#include <poppler/cpp/poppler-page.h>

using std::vector;
using std::string;

// Класс для получения страницы описания и информации из описания
class PDF_Parser {
public:
    PDF_Parser(poppler::document* doc) : doc(doc) {}
    vector<string> find_description_parts();
    int get_desc_num() { return desc_page_num; }
private:
    int find_description_page_num();
    bool get_pre_info_str(string text, int& start, int& end);
    bool get_annotation_str(string text, int& start, int& end);
    vector<string> split_tokens_by_em_dash(string str);
    string formatStr(string str);
    std::pair<string, int> getPublishAndYear(string& str);
    
    poppler::document* doc;
    int desc_page_num = -1;
};

// Находим страницу с описанием (номер)
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

// Получаем по ссылкам начальную и конечную позиции основной информации описания
// Автор, название, издание и год издания 
bool PDF_Parser::get_pre_info_str(string text, int& start, int& end) {
    // Таким образом остановимся на конце блока основной информации
    while (end > 0 && text[end] != '\n') end--;
    if (end > 0) {
        end--;
    } else return false;
    start = end;
    
    // Далее находим стартовую позицию описания, берём в учёт то, что после этого блока идут 
    // n переносов строки (нам хватит проверки на 2 подряд идущих переноса)
    while (start - 1 > 0 && !(text[start] == '\n' && text[start - 1] == '\n')) start--;
    start++;  // Инкрементом вернемся на символы после переносов строки

    // Проверка на нарушение границ строки
    return start >= 0;
}

// Получаем по ссылкам начальную и конечную позиции информации об аннотации
// Аннотация
bool PDF_Parser::get_annotation_str(string text, int& start, int& end) {
    // Проходим до конца текущей строки с номером книги и помечаем стартом начало новой строки
    while (start < text.size() && text[start - 1] != '\n') start++;
    if (start + 1 < text.size()) {
        start++;
    } else return false;
    
    end = start;
    int endPosFirst = text.find("УДК", start);
    int endPosSec = text.find("ББК", start);
    // Находим конец аннотации как начало блоков классификации
    end = std::min(endPosFirst, endPosSec);
    // В этом случае что-то пошло не так, так как не нашлись нужные блоки, вернем ошибку
    if (end >= text.size()) {
        return false;
    }
    // В итоге получаем конец последнего предложения в аннотации, пропуская все пробелы и переносы строки
    while (end >= 0 && text[end] != '.') end--;

    // В конце смотрим, не вышли ли мы за пределы текста
    return end >= 0 && start < text.size();
}

// Форматируем строку под нормальный вид. Inplace алгоритм, однако копирует исходную строку в выводе для удобства.
string PDF_Parser::formatStr(string str) {
    int ready = 0, current = 0;
    // Пропускаем пробелы в начале строки
    while (current < str.size() && isspace(str[current])) current++;
    while (current < str.size()) {
        // Убираем все переносы слов на новую строку через '-' в исходных строках файла
        if (current + 1 < str.size() && str[current] == '-' && str[current + 1] == '\n') {
            current += 2;
            while (current < str.size() && isspace(str[current])) current++; // убираем все пробелы, так как слово продолжается
        } else if (str[current] == '\n') {   // Убираем все обычные переносы
            current++;
            while (current + 1 < str.size() && isspace(str[current + 1])) current++;  // тут убираем лишние пробелы, оставляя один
        } else if (current + 1 < str.size() && str[current] == ' ' && str[current + 1] == ' ') {  // убираем повторения пробелов
            str[ready++] = str[current++];
            while (current + 1 < str.size() && isspace(str[current + 1])) current++;
        } else {
            str[ready++] = str[current++];
        }
    }

    str.resize(ready);  // Меняем размер строки, так как мы её гарантированно сократили
    return str;
}

// Не совсем обычный сплит строки по 2 байтовому символу u'–'
vector<string> PDF_Parser::split_tokens_by_em_dash(string str) {
    int start = 0, end = 0;
    vector<string> result;

    // Конвертируем строку из utf8 в utf16, где гарантированно символы имеют длину 2+ байта
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    std::u16string utf16str = converter.from_bytes(str);
    while (end < utf16str.size()) {
        if (utf16str[end] == u'–' || utf16str[end] == u'—') {  // Сравниваем текущий 2 байтовый символ с разделителем 
            result.push_back(converter.to_bytes(utf16str.substr(start, end - start)));  // записываем без учёта разделителя
            start = ++end;
        } else {
            end++;
        }
    }

    // Обработка конечной строки после последнего разделителя
    if (start != end) {
        result.push_back(converter.to_bytes(utf16str.substr(start, end - start)));
    }

    return result;
}

// Получаем пару издательство и год издательства из строки
std::pair<string, int> PDF_Parser::getPublishAndYear(string& str) {
    int start = 0, end = 0, endOfPublish = 0;
    std::pair<string, int> result;
    while (isspace(str[start])) start++;
    while (end + 3 < str.size()) {
        // Доходим до начала года издательства
        if (isdigit(str[end]) && isdigit(str[end + 1]) && isdigit(str[end + 2]) && isdigit(str[end + 3])) {
            endOfPublish = end;
            while (endOfPublish >= 0 && str[endOfPublish + 1] != ',') endOfPublish--;  // фиксируем конец названия издательства
            break;
        }
        end++;
    }

    // Строка издательства и год издания в int
    result.first = str.substr(start, endOfPublish - start + 1);
    result.second = stoi(str.substr(end, 4));

    return result;
}

// Поиск частей описания: {title, author, year, publish, annotation}
vector<string> PDF_Parser::find_description_parts() {
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

    int middleInd = 0;
    middleInd = text.find("ISBN");
    // Изначально все указатели идут на начало книжного номера, разделяющего аннотацию и прочие необходимые данные
    int preStartInd = middleInd, preEndInd = middleInd, annotationStartInd = middleInd, annotationEndInd = middleInd;
    
    // Вызываем поиск позиций и сразу проверяем их нахождение
    if (!get_annotation_str(text, annotationStartInd, annotationEndInd) || !get_pre_info_str(text, preStartInd, preEndInd)) {
        return {};
    }

    // Сразу получаем подстрокой аннотацию в массив
    result_data[4] = formatStr(text.substr(annotationStartInd, annotationEndInd - annotationStartInd + 1));  // Аннотация
    // Потом находим конец строки с именем автора и сохраняем его подстрокой в массив
    int authorEnd = preStartInd;
    while (authorEnd + 1 < text.size() && text[authorEnd + 1] != '\n') authorEnd++;
    result_data[1] = formatStr(text.substr(preStartInd, authorEnd - preStartInd + 1));  // Имя автора

    // Находим начало блока названия 
    int startOfTitle = authorEnd;
    while (startOfTitle - 2 < text.size() && text[startOfTitle - 1] != '\t' && !(text[startOfTitle - 1] == ' ' && text[startOfTitle - 2] == ' ')) startOfTitle++;
    // Делим на токены информационную часть текста 
    vector<string> descTokens = split_tokens_by_em_dash(text.substr(startOfTitle, preEndInd - startOfTitle + 1));
    result_data[0] = formatStr(descTokens[0]);  // Название книги лежит первым

    // Так как вторым токеном всегда находится издательство с годом издания, вытаскиваем эту пару методом парсинга 
    std::pair<string, int> publishPair = getPublishAndYear(descTokens[1]);
    result_data[2] = std::to_string(publishPair.second);  // Год издания
    result_data[3] = formatStr(publishPair.first);  // Название издательства

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

        // Выводим результат блоками через '|'
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
