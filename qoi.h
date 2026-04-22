#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0;
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);

static inline void QoiStore(uint8_t history[64][4], uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int idx = QoiColorHash(r, g, b, a);
    history[idx][0] = r;
    history[idx][1] = g;
    history[idx][2] = b;
    history[idx][3] = a;
}

bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    QoiWriteU32(width);
    QoiWriteU32(height);
    QoiWriteU8(channels);
    QoiWriteU8(colorspace);

    int run = 0;
    int px_num = width * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r = 0, g = 0, b = 0, a = 255u;
    uint8_t pr = 0, pg = 0, pb = 0, pa = 255u;

    for (int i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        g = QoiReadU8();
        b = QoiReadU8();
        if (channels == 4) a = QoiReadU8();

        if (r == pr && g == pg && b == pb && a == pa) {
            run++;
            if (run == 62 || i == px_num - 1) {
                QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
                run = 0;
            }
        } else {
            if (run > 0) {
                QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
                run = 0;
            }

            int idx = QoiColorHash(r, g, b, a);
            if (history[idx][0] == r && history[idx][1] == g && history[idx][2] == b && history[idx][3] == a) {
                QoiWriteU8(QOI_OP_INDEX_TAG | static_cast<uint8_t>(idx));
            } else {
                int dr = static_cast<int>(r) - static_cast<int>(pr);
                int dg = static_cast<int>(g) - static_cast<int>(pg);
                int db = static_cast<int>(b) - static_cast<int>(pb);

                if (a == pa && dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
                    uint8_t byte = QOI_OP_DIFF_TAG | static_cast<uint8_t>((dr + 2) << 4 | (dg + 2) << 2 | (db + 2));
                    QoiWriteU8(byte);
                } else if (a == pa) {
                    int dg_ = dg;
                    int dr_dg = dr - dg_;
                    int db_dg = db - dg_;
                    if (dg_ >= -32 && dg_ <= 31 && dr_dg >= -8 && dr_dg <= 7 && db_dg >= -8 && db_dg <= 7) {
                        uint8_t b1 = QOI_OP_LUMA_TAG | static_cast<uint8_t>(dg_ + 32);
                        uint8_t b2 = static_cast<uint8_t>(((dr_dg + 8) << 4) | (db_dg + 8));
                        QoiWriteU8(b1);
                        QoiWriteU8(b2);
                    } else {
                        QoiWriteU8(QOI_OP_RGB_TAG);
                        QoiWriteU8(r);
                        QoiWriteU8(g);
                        QoiWriteU8(b);
                    }
                } else {
                    QoiWriteU8(QOI_OP_RGBA_TAG);
                    QoiWriteU8(r);
                    QoiWriteU8(g);
                    QoiWriteU8(b);
                    QoiWriteU8(a);
                }

                QoiStore(history, r, g, b, a);
            }

            pr = r; pg = g; pb = b; pa = a;
        }
    }

    for (int i = 0; i < 8; ++i) QoiWriteU8(QOI_PADDING[i]);
    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {
    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') return false;

    width = QoiReadU32();
    height = QoiReadU32();
    channels = QoiReadU8();
    colorspace = QoiReadU8();

    int px_num = width * height;
    int run = 0;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r = 0, g = 0, b = 0, a = 255u;

    for (int i = 0; i < px_num; ++i) {
        if (run > 0) {
            run--;
        } else {
            uint8_t b1 = QoiReadU8();
            if (b1 == QOI_OP_RGB_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
            } else if (b1 == QOI_OP_RGBA_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
                a = QoiReadU8();
            } else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
                int idx = b1 & 0x3f;
                r = history[idx][0];
                g = history[idx][1];
                b = history[idx][2];
                a = history[idx][3];
            } else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
                int dr = ((b1 >> 4) & 0x03) - 2;
                int dg = ((b1 >> 2) & 0x03) - 2;
                int db = (b1 & 0x03) - 2;
                r = static_cast<uint8_t>(static_cast<int>(r) + dr);
                g = static_cast<uint8_t>(static_cast<int>(g) + dg);
                b = static_cast<uint8_t>(static_cast<int>(b) + db);
            } else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
                uint8_t b2 = QoiReadU8();
                int dg = (b1 & 0x3f) - 32;
                int dr_dg = ((b2 >> 4) & 0x0f) - 8;
                int db_dg = (b2 & 0x0f) - 8;
                r = static_cast<uint8_t>(static_cast<int>(r) + dg + dr_dg);
                g = static_cast<uint8_t>(static_cast<int>(g) + dg);
                b = static_cast<uint8_t>(static_cast<int>(b) + dg + db_dg);
            } else if ((b1 & QOI_MASK_2) == QOI_OP_RUN_TAG) {
                run = (b1 & 0x3f);
            }
        }

        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);

        QoiStore(history, r, g, b, a);
    }

    bool valid = true;
    for (int i = 0; i < 8; ++i) if (QoiReadU8() != QOI_PADDING[i]) valid = false;
    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
