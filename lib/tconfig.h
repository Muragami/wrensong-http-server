#ifndef _TCONFIG_H_
#define _TCONFIG_H_

#include <stdbool.h>
#include <stdarg.h>

typedef struct ini_entry_s
{
    char *key;
    char *value;
} ini_entry_s;

typedef struct ini_section_s
{
    char *name;
    ini_entry_s *entry;
    int size;
} ini_section_s;

typedef struct ini_table_s
{
    ini_section_s *section;
    int size;
} ini_table_s;

/** 
 * The supplied getc function must behave like fgetc(): 
 * https://pubs.opengroup.org/onlinepubs/007904875/functions/fgetc.html
 * The supplied error function must behave like ferror():
 * https://pubs.opengroup.org/onlinepubs/007904875/functions/ferror
**/
typedef struct ini_in
{
    int (*getc)(void *arg);
    int (*error)(void *arg);
    void *arg;
} ini_in_s;

/** 
 * The supplied vprintf function must behave like vfprintf():
 * https://pubs.opengroup.org/onlinepubs/009696699/functions/vfprintf.html
**/
typedef struct ini_out
{
    int (*vprintf)(void *arg, const char *str, va_list lst);
    void *arg;
} ini_out_s;

/** 
 * The supplied set() function is called for each key/value pair read.
 * The supplied create() function is called for each section created.
**/
typedef struct ini_callback
{
    void (*set)(void *arg, const char *key, const char *value);
    void (*create)(void *arg, const char *section);
    void *arg;
} ini_callback_s;

/**
 * @brief Type definition for error handler function
 */
typedef void (*ini_error_handler_t)(const char *error_message, void *p);

/**
 * @brief Sets an error handler function to be called on errors.
 * @param handler
 * @return void
 */
void ini_set_error_handler(ini_error_handler_t handler);

/**
 * @brief Sets a flag to exit the program on error inside the library. (default is yes = true)
 * @param yes
 * @return void
 */
void ini_set_error_exit(bool yes);

/**
 * @brief Creates an empty ini_table_s struct for writing new entries to.
 * @return ini_table_s*
 */
ini_table_s *ini_table_create(void);

/**
 * @brief Free up all the allocated resources in the ini_table_s struct.
 * @param table
 */
void ini_table_destroy(ini_table_s *table);

/**
 * @brief Creates an ini_table_s struct filled with data from the specified
 *        `file'.  Returns NULL if the file can not be read.
 * @param table
 * @param file
 * @return success
 */
bool ini_table_read_from_file(ini_table_s *table, const char *file);

/**
 * @brief Creates an ini_table_s struct filled with data using the
 *        given input struct
 * @param table
 * @param in
 * @return success
 */
bool ini_table_read(ini_table_s *table, ini_in_s *in);

/**
 * @brief Parses an ini file using the given input struct and calls the
 *        provided callbacks.
 * @param in
 * @param callback
 * @return success
 */
bool ini_read(ini_in_s *in, ini_callback_s *callback);

/**
 * @brief Parses an ini file and calls the provided callbacks.
 * @param fname
 * @param callback
 * @return success
 */
bool ini_read_file(const char *fname, ini_callback_s *callback);

/**
 * @brief Writes the specified ini_table_s struct to the specified `file'.
 *        Returns false if the file could not be opened for writing, otherwise
 *        true.
 * @param table
 * @param file
 * @return bool
 */
bool ini_table_write_to_file(ini_table_s *table, const char *file);

/**
 * @brief Writes the specified ini_table_s struct through the given
 *        output struct
 * @param table
 * @param out
 * @return bool
 */
bool ini_table_write(ini_table_s *table, ini_out_s *out);

/**
 * @brief Creates a new entry in the `table' containing the `key' and `value'
 *        provided if it does not exist.  Otherwise, modifies an exsiting `key'
 *        with the new `value'
 * @param table
 * @param section_name
 * @param key
 * @param value
 */
void ini_table_create_entry(ini_table_s *table, const char *section_name,
                            const char *key, const char *value);

/**
 * @brief Checks for the existance of an entry in the specified `table'.  Returns
 *        false if the entry does not exist, otherwise true.
 * @param table
 * @param section_name
 * @param key
 * @return bool
 */
bool ini_table_check_entry(ini_table_s *table, const char *section_name,
                           const char *key);

/**
 * @brief Retrieves the unmodified value of the specified `key' in `section_name'.
 *        Returns NULL if the entry does not exist, otherwise a pointer to the
 *        entry value data.
 * @param table
 * @param section_name
 * @param key
 * @return const char*
 */
const char *ini_table_get_entry(ini_table_s *table, const char *section_name,
                                const char *key);

/**
 * @brief Retrieves the value of the specified `key' in `section_name', converted
 *        to int.  Returns false on failure, otherwise true.
 * @param table
 * @param section_name
 * @param key
 * @param [out]value
 * @return int
 */
bool ini_table_get_entry_as_int(ini_table_s *table, const char *section_name,
                                const char *key, int *value);

/**
 * @brief Retrieves the value of the specified `key' in `section_name', converted
 *        to double.  Returns false on failure, otherwise true.
 * @param table
 * @param section_name
 * @param key
 * @param [out]value
 * @return int
 */
bool ini_table_get_entry_as_double(ini_table_s *table, const char *section_name,
                                const char *key, double *value);

/**
 * @brief Retrieves the value of the specified `key' in `section_name', converted
 *        to bool.  Returns false on failure, true otherwise.
 * @param table
 * @param section_name
 * @param key
 * @param [out]value
 * @return bool
 */
bool ini_table_get_entry_as_bool(ini_table_s *table, const char *section_name,
                                 const char *key, bool *value);

#endif //_TCONFIG_H_
