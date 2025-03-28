/*
 * utf8_xcoder.h
 *
 * Author: https://gist.github.com/tylerneylon/9773800
 *
 * // Convert a utf-8 byte array into unicode code points:
 *
 * char *utf8_bytes = get_utf8_bytes();  // The end is marked by a zero byte here.
 * int *code_points = get_unicode_buffer();
 *
 * while (( *code_points++ = decode_code_point(&utf8_bytes) ));
 *
 *
 * // Convert unicode code points into a utf8 byte array.
 *
 * int *code_points = get_unicode_code_points();  // The end is marked by a zero value.
 * int *code_point_cursor = code_points;  // Copy of the base pointer so we can move this one around.
 * char *utf8_bytes = get_utf8_buffer(BUFFER_LEN);
 * char *end_byte = utf8_bytes + BUFFER_LEN;
 *
 * do {
 *     encode_code_point(&utf_bytes, end_byte, *code_point_cursor++);
 * } while (*(code_point_cursor - 1));
 *
 */

#ifndef UTF8_XCODER_H_
#define UTF8_XCODER_H_

int  decode_code_point(char **s);
void encode_code_point(char **s, char *end, int code);
int  split_into_surrogates(int code, int *surr1, int *surr2);
int  join_from_surrogates(int *old, int *code);

void code_point_to_str(char **s, int code);
int	 str_to_code_point(char **s);

#endif /* UTF8_XCODER_H_ */
