#include <string.h>

#include "sim800_parser.h"

#include <stdio.h>

#include "sim800.h"

#define PARSE_TIMEOUT	100 /* ms */
#define DELAY_TIMEOUT	  0 /* yield */

extern Sim800Handle_t* Sim800Handle;

static void trim_crlf(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
        len--;
    }
    if (len > 0 && str[len - 1] == '\r') {
        str[len - 1] = '\0';
    }
}

static bool
is_match(const char* s1, const char* s2, size_t len)
{
    if (s1 && s2)
    {
        if (len == 0)
            return true;

        for (int i = 0; i < len; i++) {
            if (s2[i] == '\e')
                continue;
            if (s1[i] != s2[i])
                return false;
        }
        return true;
    }
    return false;
}


/**
 *
 */
void
sim800_rx_ring_parser(Sim800Handle_t* p)
{
    char source[512];
    bool match_found = false;

    if (!circular_buf_empty(p->RxCbufHandle))
    {
        sim800_readline(p, source, sizeof(source), 1000);

        // debug_printf("[%s] buffer line <%s>\n", __func__, source);

        if (source[0] != '\0') {
            if (strncmp(source, "AT+", 3) == 0 /* match */) {
            	/* AT Command echo, drop */
            	// debug_printf("< %s > drop echo: '%s'\n", __func__, source);
            } else {
                for (int i = 0; i < SIM800_PARSERS_MAX; i++) {
                    /* Processing one of targets */
                    Sim800Parser_t* parser = &p->ParsersList[i];

                    if (parser->str && parser->len)
                    {
                        if (is_match(source, parser->str, parser->len))
                        {
                            match_found = true;
                            // debug_printf("Parse match: %s\n", parser->str);
                            if (parser->handler)
                                parser->handler(Sim800Handle, source, parser->handler_param);
                        }
                    }
                }
            }

            if (!match_found) {
            	// debug_printf("< %s > drop: '%s'\n", __func__, source);
            }
        }
        // debug_printf("< %s > parsed: '%s'\n", __func__, source);
    }

    /* Check command execution timeout */
    if (sim800_is_locked(p)) {
        if ((SIM800_GET_TICK() - p->Command.ts) >= p->Command.timeout) {
            sim800_unlock(p);
            #ifdef DEBUG
            char tmp_buf[128];
            snprintf(tmp_buf, sizeof(tmp_buf), p->Command.str);
            trim_crlf(tmp_buf);
            debug_printf("[SIM800] Warning: Cmd <%s> TIMEOUT!\n", tmp_buf);
            #endif
            if (p->Command.callback) {
                p->Command.callback(p, SIM800_EVENT_CMD_RESULT_TIMEOUT, p->Command.callback_param);
            }
        }
    }
}

void sim800_readline(Sim800Handle_t* p, char* str, size_t size, uint32_t timeout)
{
    uint32_t ts = SIM800_GET_TICK(); // Получаем текущее время

    int index = 0; // Индекс для записи символов в строку
    uint8_t byte;  // Переменная для хранения текущего байта

    for (;;) { // Бесконечный цикл для чтения данных
        while (circular_buf_get(p->RxCbufHandle, &byte) == (-1)) {
            if ((SIM800_GET_TICK() - ts) < timeout) {
                SIM800_DELAY_MS(DELAY_TIMEOUT);
            }
            else {
                str[0] = '\0';
                return;
            }
        }
        if (index == 1 && byte == ' ' && str[0] == '>') {
            str[1] = byte;
            str[2] = '\0';
            return; // Приглашение на отправку данных
        }
        if (index > 0 && byte == '\n' && str[index - 1] == '\r') {
            str[index - 1] = '\0'; // Заменяем CR на завершающий ноль
            return; // Успешное чтение строки
        }
        else if (index >= size - 1) {
            // Если буфер почти заполнен, сдвигаем содержимое влево и добавляем новый байт
            memmove(str, str + 1, size - 2);
            str[size - 2] = byte;
            str[size - 1] = '\0';
            index = size - 1;
        }
        else {
            str[index++] = byte;
        }
    }
}

/**
 *
 */
int sim800_parse_int(const char** pptr)
{
    bool negative = false;
    int result = 0;

    if (
        **pptr == ' '||
        **pptr == '"'||
        **pptr == ','||
        **pptr == '.'||
        **pptr == '+')
    {
        *pptr += 1;
    }
    if (**pptr == '-')
    {
        negative = true;
        *pptr += 1;
    }
    while (**pptr >= '0' && **pptr <= '9')
    {
        result = result * 10 + (**pptr - '0');
        *pptr += 1;
    }
    return result * ((negative) ? (-1) : 1);
}

/**
 *
 */
void
sim800_parse_str(const char** pptr, char* str)
{
    bool ready = false;

    if (
        **pptr == ' '||
        **pptr == ','||
        **pptr == '.')
    {
        *pptr += 1;
    }
    if (**pptr == '"')
    {
        *pptr += 1;
    }
    while (
        **pptr != '\r'&&
        **pptr != '\n'&&
        **pptr != '"')
    {
        *str++ = **pptr;
        *pptr += 1;
    }
    *str = '\0'; /* terminate */
}

///**
// *
// */
//void
//sim800_str_parser(sim800_t* p, void* param)
//{
//    char* str = (char*)param;
//
//    uint8_t byte;
//
//    int index = -1;
//
//    uint32_t ts = SIM800_GET_TICK();
//
//    for (;;) {
//        while (circular_buf_get(p->RxCbufHandle, &byte) == (-1)) {
//            if ((SIM800_GET_TICK() - ts) < PARSE_TIMEOUT) {
//                SIM800_DELAY_MS(DELAY_TIMEOUT);
//            }
//            else {
//                return; // TIMEOUT...
//            }
//        }
//        /* Skip leading symbols */
//        if (index < 0 && (byte == '\r'||
//                          byte == '\n'||
//                          byte == ' ' )) {
//            continue;
//        }
//        if (index < 0 && (byte == '"')) {
//            index = 0;
//            continue;
//        }
//        if (
//            byte != '"' &&
//            byte != '\r'&&
//            byte != '\n' )
//        {
//            if (index < 0) {
//                index = 0;
//            }
//            str[index++] = byte;
//        }
//        else
//        {
//            str[index] = 0x00; // End of string
//            //            printf("str '%s' (%d)\n", str, index);
//            return; // Ok
//        }
//    }
//}
//
//
///**
// *
// */
//void
//sim800_num_parser(sim800_t* p, void* param)
//{
//    int* num = (int*)param;
//
//    uint8_t byte;
//
//    int value = 0;
//    bool negative = false;
//
//    int index = 0;
//    uint32_t ts = SIM800_GET_TICK();
//
//    for (;;) {
//        while (circular_buf_get(p->RxCbufHandle, &byte) == (-1)) {
//            if ((SIM800_GET_TICK() - ts) < PARSE_TIMEOUT) {
//                SIM800_DELAY_MS(DELAY_TIMEOUT);
//            }
//            else {
//                return; // TIMEOUT...
//            }
//        }
//        /* Skip leading symbols */
//        if (index == 0 && (byte == '"'||
//                           byte == ' '||
//                           byte == '+'||
//                           byte == ':'||
//                           byte == '/'))
//        {
//            continue;
//        }
//        else if (byte == '-') {
//            negative = true;
//            continue;
//        }
//
//        if (byte >= '0' && byte <= '9' )
//        {
//            value = value * 10 + byte - '0';
//            index++;
//        }
//        else
//        {
//            *num = value * ((negative) ? -1 : 1);
//            //            printf("int '%d' (%d)\n", *num, index);
//            return; // Ok
//        }
//    }
//}
//
//
///**
// *
// */
//void
//sim800_skip_parse(sim800_t* p, int size)
//{
//    uint8_t byte;
//
//    int index = 0;
//    uint32_t ts = SIM800_GET_TICK();
//
//    for (;;) {
//        while (circular_buf_get(p->RxCbufHandle, &byte) == (-1)) {
//            if ((SIM800_GET_TICK() - ts) < PARSE_TIMEOUT) {
//                SIM800_DELAY_MS(DELAY_TIMEOUT);
//            }
//            else {
//                return; // TIMEOUT...
//            }
//        }
//        if (index < size) {
//            index++;
//        }
//        else {
//            //            printf("skip: %d'\n", size);
//            return; // Ok
//        }
//    }
//}


/**
 *
 */
void
sim800_ip_addr_parse(Sim800Handle_t* p, void* param)
{
    uint8_t* u8 = (uint8_t*)param;

    int octet[4];

    //    sim800_num_parser(p, &octet[3]);
    //    sim800_skip_parse(p, 1 /* [ . ] */);
    //    sim800_num_parser(p, &octet[2]);
    //    sim800_skip_parse(p, 1 /* [ . ] */);
    //    sim800_num_parser(p, &octet[1]);
    //    sim800_skip_parse(p, 1 /* [ . ] */);
    //    sim800_num_parser(p, &octet[0]);
    //
    //    if (u8) {
    //        u8[0] = (uint8_t)octet[0];
    //        u8[1] = (uint8_t)octet[1];
    //        u8[2] = (uint8_t)octet[2];
    //        u8[3] = (uint8_t)octet[3];
    //    }
    //
    //    printf("IP: '%d.%d.%d.%d'\n",
    //           octet[3], octet[2], octet[1], octet[0]);

    return; // Ok
}
