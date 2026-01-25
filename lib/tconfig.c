/*
MIT License

Copyright (c) 2019 Justin Kinnaird
modified 2026 Jason A. Petrasko

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tconfig.h"

static ini_error_handler_t on_error = NULL;
static bool exit_on_error = true;

static void _ini_error(const char *error_message, void *p)
{
    if (on_error != NULL)
    {
        on_error(error_message, p);
    }
    else
    {
        fprintf(stderr, "TConfig Error: %s\n", error_message);
        if (exit_on_error)
            exit(EXIT_FAILURE);
    }
}

static ini_entry_s *_ini_entry_create(ini_callback_s *call, ini_section_s *section,
                                      const char *key, const char *value)
{
    if (call && call->set)
    {
        call->set(call->arg, key, value);
        return NULL;
    }
    if ((section->size % 10) == 0)
    {
        section->entry =
            realloc(section->entry, (10 + section->size) * sizeof(ini_entry_s));
        if (section->entry == NULL)
        {
            _ini_error("Failed to allocate memory for ini entry", section);
        }
    }
    ini_entry_s *entry = &section->entry[section->size++];
    entry->key = strdup(key);
    entry->value = strdup(value);
    if (entry->key == NULL || entry->value == NULL)
    {
        _ini_error("Failed to allocate memory for ini entry key/value", section);
    }
    return entry;
}

static ini_section_s *_ini_section_create(ini_callback_s *call, ini_table_s *table,
                                          const char *section_name)
{
    if (call && call->create)
    {
        call->create(call->arg, section_name);
        return NULL;
    }
    if ((table->size % 10) == 0)
    {
        table->section =
            realloc(table->section, (10 + table->size) * sizeof(ini_section_s));
        if (table->section == NULL)
        {
            _ini_error("Failed to allocate memory for ini section", table);
        }
    }
    ini_section_s *section = &table->section[table->size++];
    section->size = 0;
    section->name = strdup(section_name);
    if (section->name == NULL)
    {
        _ini_error("Failed to allocate memory for ini section name", table);
        return section;
    }
    section->entry = calloc(1, 10 * sizeof(ini_entry_s));
    if (section->entry == NULL)
    {
        _ini_error("Failed to allocate memory for ini section entries", table);
    }
    return section;
}

static ini_section_s *_ini_section_find(ini_table_s *table, const char *name)
{
    for (int i = 0; i < table->size; i++)
    {
        if (!strcmp(table->section[i].name, name))
        {
            return &table->section[i];
        }
    }
    return NULL;
}

static ini_entry_s *_ini_entry_find(ini_section_s *section, const char *key)
{
    for (int i = 0; i < section->size; i++)
    {
        if (!strcmp(section->entry[i].key, key))
        {
            return &section->entry[i];
        }
    }
    return NULL;
}

static ini_entry_s *_ini_entry_get(ini_table_s *table, const char *section_name,
                                   const char *key)
{
    ini_section_s *section = _ini_section_find(table, section_name);
    if (section == NULL)
    {
        return NULL;
    }
    ini_entry_s *entry = _ini_entry_find(section, key);
    if (entry == NULL)
    {
        return NULL;
    }
    return entry;
}

void ini_set_error_exit(bool yes)
{
    exit_on_error = yes;
}

void ini_set_error_handler(ini_error_handler_t handler)
{
    on_error = handler;
}

ini_table_s *ini_table_create(void)
{
    ini_table_s *table = calloc(1, sizeof(ini_table_s));
    if (table == NULL)
    {
        _ini_error("Failed to allocate memory for ini table", NULL);
    }
    table->size = 0;
    table->section = calloc(1, 10 * sizeof(ini_section_s));
    if (table->section == NULL)
    {
        _ini_error("Failed to allocate memory for ini table sections", table);
    }
    return table;
}

void ini_table_destroy(ini_table_s *table)
{
    for (int i = 0; i < table->size; i++)
    {
        ini_section_s *section = &table->section[i];
        free(section->entry);
    }
    free(table->section);
    free(table);
}

bool _ini_read(ini_in_s *in, ini_callback_s *call, ini_table_s *table)
{
    char error[256];

    enum
    {
        Section,
        Key,
        Value,
        Comment
    } state = Section;
    int c;
    unsigned position = 0;
    int spaces = 0;
    int line = 0;
    size_t buffer_size = 128 * sizeof(char);
    char *buf = calloc(1, buffer_size);
    char *value = NULL;
    if (buf == NULL)
    {
        _ini_error("Failed to allocate memory for ini parsing buffer", table);
        return false;
    }

    
    ini_section_s *current_section = NULL;

    while ((c = in->getc(in->arg)) != EOF)
    {
        if (position > buffer_size - 2)
        {
            buffer_size += 128 * sizeof(char);
            size_t value_offset = value == NULL ? 0 : value - buf;
            char *res = realloc(buf, buffer_size);
            if (!res)
            {
                free(buf);
                _ini_error("Failed to allocate memory for ini parsing buffer", table);
                break;
            }
            else
            {
                buf = res;
            }
            memset(buf + position, '\0', buffer_size - position);
            if (value != NULL)
                value = buf + value_offset;
        }
        switch (c)
        {
        case ' ':
            switch (state)
            {
            case Value:
                if (value[0] != '\0')
                    spaces++;
                break;
            default:
                if (buf[0] != '\0')
                    spaces++;
                break;
            }
            break;
        case ';':
        case '#':
            if (state == Value)
            {
                buf[position++] = c;
                break;
            }
            else
            {
                state = Comment;
                buf[position++] = c;
                while (c != EOF && c != '\n')
                {
                    c = in->getc(in->arg);
                    if (c != EOF && c != '\n')
                        buf[position++] = c;
                }
            }
        case '\r':
        // ignore this character
            break;
        case '\n':
        // fallthrough
        case EOF:
            line++;
            if (state == Value)
            {
                if (current_section == NULL)
                {
                    current_section = _ini_section_create(call, table, "");
                }
                _ini_entry_create(call, current_section, buf, value);
                value = NULL;
            }
            else if (state == Comment)
            {
                if (current_section == NULL)
                {
                    current_section = _ini_section_create(call, table, "");
                }
                _ini_entry_create(call, current_section, buf, "");
            }
            else if (state == Section)
            {
                snprintf(error, 256,
                         "Section `%s' missing `]' operator.", buf);
                _ini_error(error, table);
            }
            else if (state == Key && position)
            {
                snprintf(error, 256,
                         "Key `%s' missing `=' operator.", buf);
                _ini_error(error, table);
            }
            memset(buf, '\0', buffer_size);
            state = Key;
            position = 0;
            spaces = 0;
            break;
        case '[':
            state = Section;
            break;
        case ']':
            current_section = _ini_section_create(call, table, buf);
            memset(buf, '\0', buffer_size);
            position = 0;
            spaces = 0;
            state = Key;
            break;
        case '=':
        case ':':
            if (state == Key)
            {
                state = Value;
                buf[position++] = '\0';
                value = buf + position;
                spaces = 0;
                continue;
            }
        default:
            for (; spaces > 0; spaces--)
                buf[position++] = ' ';
            buf[position++] = c;
            break;
        }
    }
    if (in->error(in->arg))
    {
        _ini_error("Error reading ini file", table);
    }
    free(buf);
    return true;
}

bool ini_read(ini_in_s *in, ini_callback_s *callback)
{
    return _ini_read(in, callback, NULL);
}

bool ini_read_file(const char *fname, ini_callback_s *callback)
{
    FILE *f = fopen(fname, "r");
    if (f == NULL)
    {
        _ini_error("Failed to open ini file for reading", NULL);
        return false;
    }
    ini_in_s fio;
    fio.getc = (int (*)(void *))fgetc;
    fio.error = (int (*)(void *))ferror;
    fio.arg = f;
    return ini_read(&fio, callback);
}

bool ini_table_read(ini_table_s *table, ini_in_s *in)
{
    return _ini_read(in, NULL, table);
}

bool ini_table_read_from_file(ini_table_s *table, const char *file)
{
    FILE *f = fopen(file, "r");
    if (f == NULL)
    {
        _ini_error("Failed to open ini file for reading", table);
        return false;
    }
    ini_in_s fio;
    fio.getc = (int (*)(void *))fgetc;
    fio.error = (int (*)(void *))ferror;
    fio.arg = f;
    bool result = ini_table_read(table, &fio);
    fclose(f);
    return result;
}

static void _ini_write(ini_table_s *table, ini_out_s *out, const char *str, ...)
{
    va_list args;
    va_start(args, str);
    if (out->vprintf(out->arg, str, args) < 0)
    {
        _ini_error("Failed to write to ini file", table);
        va_end(args);
        return;
    }
    va_end(args);
}

bool ini_table_write(ini_table_s *table, ini_out_s *out)
{
    for (int i = 0; i < table->size; i++)
    {
        ini_section_s *section = &table->section[i];
        _ini_write(table, out, i > 0 ? "\n[%s]\n" : "[%s]\n", section->name);
        for (int q = 0; q < section->size; q++)
        {
            ini_entry_s *entry = &section->entry[q];
            if (entry->key[0] == ';')
            {
                _ini_write(table, out, "%s\n", entry->key);
            }
            else
            {
                _ini_write(table, out, "%s = %s\n", entry->key, entry->value);
            }
        }
    }
    return true;
}

bool ini_table_write_to_file(ini_table_s *table, const char *file)
{
    FILE *f = fopen(file, "w+");
    if (f == NULL)
    {
        _ini_error("Failed to open ini file for writing", table);
        return false;
    }
    ini_out_s fio;
    fio.vprintf = (int (*)(void *, const char *, va_list))vfprintf;
    fio.arg = f;
    for (int i = 0; i < table->size; i++)
    {
        ini_section_s *section = &table->section[i];
        _ini_write(table, &fio, i > 0 ? "\n[%s]\n" : "[%s]\n", section->name);
        for (int q = 0; q < section->size; q++)
        {
            ini_entry_s *entry = &section->entry[q];
            if (entry->key[0] == ';')
            {
                _ini_write(table, &fio, "%s\n", entry->key);
            }
            else
            {
                _ini_write(table, &fio, "%s = %s\n", entry->key, entry->value);
            }
        }
    }
    fclose(f);
    return true;
}

void ini_table_create_entry(ini_table_s *table, const char *section_name,
                            const char *key, const char *value)
{
    ini_section_s *section = _ini_section_find(table, section_name);
    if (section == NULL)
    {
        section = _ini_section_create(NULL, table, section_name);
    }
    ini_entry_s *entry = _ini_entry_find(section, key);
    if (entry == NULL)
    {
        entry = _ini_entry_create(NULL, section, key, value);
    }
    else
    {
        entry->value = strdup(value);
    }
}

bool ini_table_check_entry(ini_table_s *table, const char *section_name,
                           const char *key)
{
    return (_ini_entry_get(table, section_name, key) != NULL);
}

const char *ini_table_get_entry(ini_table_s *table, const char *section_name,
                                const char *key)
{
    ini_entry_s *entry = _ini_entry_get(table, section_name, key);
    if (entry == NULL)
    {
        return NULL;
    }
    return entry->value;
}

bool ini_table_get_entry_as_int(ini_table_s *table, const char *section_name,
                                const char *key, int *value)
{
    const char *val = ini_table_get_entry(table, section_name, key);
    if (val == NULL)
    {
        return false;
    }
    *value = atoi(val);
    return true;
}

bool ini_table_get_entry_as_double(ini_table_s *table, const char *section_name,
                                   const char *key, double *value)
{
    const char *val = ini_table_get_entry(table, section_name, key);
    if (val == NULL)
    {
        return false;
    }
    *value = atof(val);
    return true;
}

bool ini_table_get_entry_as_bool(ini_table_s *table, const char *section_name,
                                 const char *key, bool *value)
{
    const char *val = ini_table_get_entry(table, section_name, key);
    if (val == NULL)
    {
        return false;
    }
    if (!strcmp(val, "on") || !strcmp(val, "true"))
    {
        *value = true;
    }
    else
    {
        *value = false;
    }
    return true;
}
