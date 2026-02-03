#pragma once
struct Localization_Text {
    Localization_Text();
    ~Localization_Text();

    void abort();

    void* mo_data;
    int reversed;

    int num_strings;
    int original_table_offset;
    int translated_table_offset;
    int hash_num_entries;
    int hash_offset;
};

char* text_lookup(char* s);