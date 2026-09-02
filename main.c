#include <gint/display.h>
#include <gint/keyboard.h>
#include <string.h>

extern int Serial_Open(unsigned char *mode);
extern int Serial_Close(int mode);
extern int Serial_ReadOneByte(unsigned char *out);
extern int Serial_BufferedTransmitOneByte(unsigned char b);
extern int Serial_ClearTransmitBuffer(void);

#define STX 0x02
#define ETX 0x03
#define MAX_LEN 32

static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ .";

static unsigned char checksum(unsigned char len, const unsigned char *data)
{
    int sum = len;
    for (int i = 0; i < len; i++) sum += data[i];
    return (unsigned char)((256 - (sum % 256)) % 256);
}

static void send_frame(const char *str)
{
    unsigned char len = (unsigned char)strlen(str);
    Serial_ClearTransmitBuffer();
    Serial_BufferedTransmitOneByte(STX);
    Serial_BufferedTransmitOneByte(len);
    for (unsigned char i = 0; i < len; i++)
        Serial_BufferedTransmitOneByte((unsigned char)str[i]);
    Serial_BufferedTransmitOneByte(checksum(len, (const unsigned char *)str));
    Serial_BufferedTransmitOneByte(ETX);
}

static int read_byte_timeout(unsigned char *out, int max_tries)
{
    for (int i = 0; i < max_tries; i++) {
        if (Serial_ReadOneByte(out) == 0) return 1;
    }
    return 0;
}

static int receive_frame(char *out, int max_out)
{
    unsigned char b;
    int timeout = 2000000;

    while (1) {
        if (!read_byte_timeout(&b, timeout)) return 0;
        if (b == STX) break;
    }

    unsigned char len;
    if (!read_byte_timeout(&len, timeout)) return 0;
    if (len >= (unsigned char)max_out) return 0;

    unsigned char payload[MAX_LEN];
    for (unsigned char i = 0; i < len; i++) {
        if (!read_byte_timeout(&payload[i], timeout)) return 0;
    }

    unsigned char chk;
    if (!read_byte_timeout(&chk, timeout)) return 0;

    unsigned char etx;
    if (!read_byte_timeout(&etx, timeout)) return 0;
    if (etx != ETX) return 0;
    if (chk != checksum(len, payload)) return 0;

    memcpy(out, payload, len);
    out[len] = '\0';
    return 1;
}

static void draw_picker(const char *buf, int cursor)
{
    dclear(C_WHITE);
    dtext(2, 2, C_BLACK, "String Reverse Test");
    dtext(2, 14, C_BLACK, buf);
    dtext(2, 30, C_BLACK, "Arrows: move  EXE: pick");
    dtext(2, 42, C_BLACK, "DEL: back  F1: send");

    int len = (int)strlen(alphabet);
    for (int i = 0; i < len; i++) {
        int x = 2 + (i % 14) * 8;
        int y = 54 + (i / 14) * 10;
        char c[2] = { alphabet[i], 0 };
        if (i == cursor) drect(x - 1, y - 1, x + 7, y + 8, C_BLACK);
        dtext(x, y, i == cursor ? C_WHITE : C_BLACK, c);
    }
    dupdate();
}

int main(void)
{
    unsigned char mode[6] = { 0, 5, 0, 0, 0, 0 };
    Serial_Open(mode);

    char buf[MAX_LEN + 1] = "";
    int cursor = 0;
    int alen = (int)strlen(alphabet);

    while (1) {
        draw_picker(buf, cursor);
        key_event_t ev = getkey();

        if (ev.key == KEY_EXIT) break;
        if (ev.key == KEY_LEFT) cursor = (cursor - 1 + alen) % alen;
        if (ev.key == KEY_RIGHT) cursor = (cursor + 1) % alen;
        if (ev.key == KEY_UP) cursor = (cursor - 14 + alen) % alen;
        if (ev.key == KEY_DOWN) cursor = (cursor + 14) % alen;

        if (ev.key == KEY_EXE) {
            int blen = (int)strlen(buf);
            if (blen < MAX_LEN) {
                buf[blen] = alphabet[cursor];
                buf[blen + 1] = '\0';
            }
        }

        if (ev.key == KEY_DEL) {
            int blen = (int)strlen(buf);
            if (blen > 0) buf[blen - 1] = '\0';
        }

        if (ev.key == KEY_F1 && strlen(buf) > 0) {
            send_frame(buf);

            dclear(C_WHITE);
            dtext(2, 2, C_BLACK, "Waiting for reply...");
            dupdate();

            char reply[MAX_LEN + 1];
            if (receive_frame(reply, sizeof(reply))) {
                dclear(C_WHITE);
                dtext(2, 2, C_BLACK, "Sent:");
                dtext(2, 14, C_BLACK, buf);
                dtext(2, 30, C_BLACK, "Received:");
                dtext(2, 42, C_BLACK, reply);
                dtext(2, 58, C_BLACK, "EXIT: new word");
                dupdate();
            } else {
                dclear(C_WHITE);
                dtext(2, 2, C_BLACK, "No valid reply");
                dtext(2, 14, C_BLACK, "EXIT: try again");
                dupdate();
            }

            getkey();
            buf[0] = '\0';
        }
    }

    Serial_Close(0);
    return 1;
}
