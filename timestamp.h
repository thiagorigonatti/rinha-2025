//
// Created by thiagorigonatti on 02/08/25.
//

#ifndef TIMESTAMP_H
#define TIMESTAMP_H
int add_requested_at( const char *json_input, char *output, size_t output_size, double unix_time);
double current_time_unix();
void generate_requested_at_field(char *buffer, size_t buffer_size, double unix_time);
double iso_to_unix(const char *datetime);
#endif
