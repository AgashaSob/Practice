#include <iostream>
#include <poppler-document.h>
#include <poppler/cpp/poppler-page.h>

using std::string;

// Класс для получения страницы описания
class PDF_Parser {
public:
    PDF_Parser(poppler::document* doc) : doc(doc) {}
    int find_description_page_num();
    int get_desc_num() { return desc_page_num; }
private:
    poppler::document* doc;
    int desc_page_num;
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

int main() {
    string filename = "somepdf.pdf";
    string filename2 = "Dauni_A._Osnovy_Python.pdf";
    string filename3 = "CHistaya_arhitektura_Iskusstvo_razrabotki.pdf";
    // Открываем PDF-документ
    poppler::document* doc = poppler::document::load_from_file("../" + filename3);
    PDF_Parser parserObj(doc);

    if (doc != nullptr) {
        int desc_page_num = parserObj.find_description_page_num();

        // Освобождаем ресурсы документа
        delete doc;

        // Выводим результат
        if (desc_page_num != -1) {
            std::cout << "Страница с описанием книги: " << desc_page_num << std::endl;
        } else {
            std::cout << "Страница описания не найдена." << std::endl;
        }
    } else {
        std::cout << "Не удалось открыть PDF-документ." << std::endl;
    }

    return 0;
}
