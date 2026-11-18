/* L2 校准：α + 开路 + 双负载两点法，Flash sector6/7 A/B 记录式存储。
 * 依据 docs/软件架构.md §7.1–§7.7、§8.2、§13.4。
 *
 * 精度纪律（§8.2）：所有求解在 double 里做，float 只用于对外 API 的入参/出参。
 * 10 Ω 档 Zm−Zs 相消需要 4.25 位十进制有效数字，float 的 7 位够用，
 * 但中间量（Zp²、W1−W2）必须 double，否则条件数判据会误判。
 *
 * 掉电语义（§7.6，必须按此实现）：
 *   ① 编程窗口掉电 → 该槽 CRC 失败被跳过，启动取 seq 最大且 CRC 通过者；
 *   ② 擦除窗口掉电 → 被擦扇区内容不确定，但另一扇区完整，启动自动回退；
 *   ③ 两扇区都无有效记录 → 名义表 + cal_level=0 + cal_stale。
 * 因此：只在"当前扇区写满"时才擦另一扇区，擦除期间旧记录始终 intact。 */

#include "cal.h"
#include "board.h"
#include "dds.h"
#include "relay.h"
#include "adc.h"
#include "dsp.h"
#include "error.h"
#include "uart.h"

#include <math.h>
#include <string.h>
#include <stddef.h>

/* ---- Flash 记录（§7.6：24 + 64 + 384×3 = 1240 B，double 权威副本）-------- */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t method;      /* 0=全部组合 method0；1=存在退化组合 */
    uint16_t flags;       /* CAL_FLAG_* */
    uint16_t pad;
    uint32_t timestamp;   /* 无 RTC：本次上电以来的 ms（诊断用，排序靠 seq） */
    uint32_t seq;
    uint32_t crc32;       /* 覆盖前 1236 B */
    double   alpha[FREQ_NUM][2];
    double   k[FREQ_NUM][RANGE_NUM][2];
    double   zs[FREQ_NUM][RANGE_NUM][2];
    double   zp[FREQ_NUM][RANGE_NUM][2];
} cal_blob_t;

#define CAL_FLAG_STALE     0x0001U
#define CAL_FLAG_BENCH_OK  0x0002U
#define CAL_FLAG_OPEN_BAD  0x0004U
#define CAL_FLAG_LOAD_BAD  0x0008U

/* 编译期确认布局与 §7.6 的 1240 B 一致（槽 1536 B 留对齐余量）。 */
typedef char cal_blob_size_check[(sizeof(cal_blob_t) == CFG_CAL_BLOB_SIZE) ? 1 : -1];

/* ---- 模块状态 ----------------------------------------------------------- */
static cal_blob_t   cal_blob;                 /* 权威 double 副本 */
static cal_record_t cal_rec;                  /* §3.4 的 float 视图 */
static uint32_t     cal_seq;                  /* 当前生效记录的 seq */
static int          cal_bank = -1;            /* 0=A(sector6) 1=B(sector7) */
static int          cal_slot = -1;            /* 当前生效槽 */
static int          cal_append_bank;          /* 下次追加的扇区 */
static int          cal_append_slot;          /* 下次追加的槽（=首个空白槽） */
static uint32_t     cal_writes;
static int          cal_stale = 1;            /* 未校准即视为 stale */
static int          cal_bench;
static uint32_t     cal_diag;                 /* 本模块产生的诊断位 */
static float        cal_stray_pf;             /* 夹具杂散指纹（10 kHz/10 kΩ） */

static const float cal_rf_nom[CFG_RANGE_NUM] = CFG_RF_NOM_INITIALIZER;
static const float cal_cf_nom[CFG_RANGE_NUM] = CFG_CF_NOM_INITIALIZER;
static const uint8_t cal_legal[CFG_RANGE_NUM][CFG_FREQ_NUM] = CFG_RANGE_FREQ_LEGAL_INITIALIZER;
static const float cal_load_std[CFG_RANGE_NUM][2] = CFG_LOAD_STD_INITIALIZER;
static const cfg_band_t cal_bands[CFG_FREQ_NUM] = CFG_BANDS_INITIALIZER;

/* ---- double 复数小工具（static，纯数学）--------------------------------- */
typedef struct { double re, im; } cdbl_t;

static cdbl_t cd(double re, double im) { cdbl_t z; z.re = re; z.im = im; return z; }
static cdbl_t cd_add(cdbl_t a, cdbl_t b) { return cd(a.re + b.re, a.im + b.im); }
static cdbl_t cd_sub(cdbl_t a, cdbl_t b) { return cd(a.re - b.re, a.im - b.im); }
static cdbl_t cd_mul(cdbl_t a, cdbl_t b)
{
    return cd(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
}
static cdbl_t cd_scale(cdbl_t a, double s) { return cd(a.re * s, a.im * s); }
static cdbl_t cd_neg(cdbl_t a) { return cd(-a.re, -a.im); }
static double cd_mag2(cdbl_t a) { return a.re * a.re + a.im * a.im; }
static double cd_mag(cdbl_t a) { return sqrt(cd_mag2(a)); }
static cdbl_t cd_div(cdbl_t a, cdbl_t b)
{
    const double d = cd_mag2(b);
    if (d == 0.0) {
        return cd(0.0, 0.0);
    }
    return cd((a.re * b.re + a.im * b.im) / d, (a.im * b.re - a.re * b.im) / d);
}
static cdbl_t cd_par(cdbl_t a, cdbl_t b)     /* a ∥ b */
{
    const cdbl_t s = cd_add(a, b);
    if (cd_mag2(s) == 0.0) {
        return cd(0.0, 0.0);
    }
    return cd_div(cd_mul(a, b), s);
}
static cdbl_t cf2d(cfloat_t a) { return cd((double)a.re, (double)a.im); }
static cfloat_t cd2f(cdbl_t a) { cfloat_t z; z.re = (float)a.re; z.im = (float)a.im; return z; }

/* ---- CRC32（IEEE，反射多项式，无表：1240 B×8 bit 足够快）---------------- */
static uint32_t cal_crc32_update(uint32_t crc, const uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0U; i < n; ++i) {
        crc ^= (uint32_t)p[i];
        for (uint32_t b = 0U; b < 8U; ++b) {
            crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return crc;
}

static uint32_t cal_blob_crc(const cal_blob_t *b)
{
    /* 覆盖除 crc32 字段本身外的全部 1236 B：
     * 头部 [0,20) + 载荷 [24,1240)。crc32 在偏移 20，占 4 B。 */
    const uint8_t *p = (const uint8_t *)b;
    const uint32_t off = (uint32_t)offsetof(cal_blob_t, crc32);
    uint32_t crc = 0xFFFFFFFFU;
    crc = cal_crc32_update(crc, p, off);
    crc = cal_crc32_update(crc, p + off + 4U, (uint32_t)sizeof(cal_blob_t) - off - 4U);
    return crc ^ 0xFFFFFFFFU;
}

/* ---- Flash 底层（.ramfunc：擦写期间不能从 Flash 取指）-------------------
 * 必须 long_call：Flash(0x08000000) → SRAM(0x20000000) 相距 384 MB，
 * 超出 BL 的 ±16 MB 范围，需要间接跳转 veneer。
 * 函数体内不得调用任何 Flash 驻留函数（含 memcpy/printf/board_delay_ms）；
 * 喂狗与计时只用寄存器写 + DWT 轮询。中断全程关闭。 */
#define CAL_FLASH_KEY1 0x45670123U
#define CAL_FLASH_KEY2 0xCDEF89ABU
#define CAL_IWDG_FEED  0x0000AAAAU
#define CAL_FEED_CYCLES (CFG_SYSCLK_HZ / 10U)   /* 每 100 ms 喂一次狗 */

/* .ramfunc/long_call 仅 ARM 有效；host 编译（金标准自测）退回普通函数，
 * 与 dsp.c 的 DSP_CCM 同一套路，保证 cal.c 可在 PC 上跑纯算法。 */
#if defined(__arm__)
#define CAL_RAMFUNC __attribute__((section(".ramfunc"), long_call, noinline))
#else
#define CAL_RAMFUNC __attribute__((noinline))
#endif

CAL_RAMFUNC
static int cal_flash_op(uint32_t addr, uint32_t snb, int erase,
                        const uint32_t *src, uint32_t nwords)
{
    const uint32_t prim = __get_PRIMASK();
    __disable_irq();

    int rc = 0;
    uint32_t last_feed = DWT->CYCCNT;

    if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
        FLASH->KEYR = CAL_FLASH_KEY1;
        FLASH->KEYR = CAL_FLASH_KEY2;
    }
    if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
        rc = -1;
        goto out;
    }

    /* 清历史错误标志（写 1 清） */
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_OPERR | FLASH_SR_WRPERR |
                FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR;

    if (erase != 0) {
        FLASH->CR = (FLASH->CR & ~(FLASH_CR_PSIZE | FLASH_CR_SNB | FLASH_CR_MER))
                  | FLASH_CR_SER | FLASH_CR_PSIZE_1 | (snb << 3U);
        FLASH->CR |= FLASH_CR_STRT;
        while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
            if ((DWT->CYCCNT - last_feed) >= CAL_FEED_CYCLES) {
                IWDG->KR = CAL_IWDG_FEED;      /* 整扇区擦除 max 4 s > 狗 2 s */
                last_feed = DWT->CYCCNT;
            }
        }
        FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB);
        if ((FLASH->SR & (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR |
                          FLASH_SR_PGSERR | FLASH_SR_OPERR)) != 0U) {
            rc = -1;
            goto out;
        }
        FLASH->SR = FLASH_SR_EOP;
    }

    if ((nwords > 0U) && (src != 0)) {
        FLASH->CR = (FLASH->CR & ~(FLASH_CR_PSIZE | FLASH_CR_SER | FLASH_CR_MER))
                  | FLASH_CR_PG | FLASH_CR_PSIZE_1;
        for (uint32_t i = 0U; i < nwords; ++i) {
            while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
                if ((DWT->CYCCNT - last_feed) >= CAL_FEED_CYCLES) {
                    IWDG->KR = CAL_IWDG_FEED;
                    last_feed = DWT->CYCCNT;
                }
            }
            *(volatile uint32_t *)(addr + 4U * i) = src[i];
        }
        while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
            if ((DWT->CYCCNT - last_feed) >= CAL_FEED_CYCLES) {
                IWDG->KR = CAL_IWDG_FEED;
                last_feed = DWT->CYCCNT;
            }
        }
        FLASH->CR &= ~FLASH_CR_PG;
        if ((FLASH->SR & (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR |
                          FLASH_SR_PGSERR | FLASH_SR_OPERR)) != 0U) {
            rc = -1;
            goto out;
        }
        FLASH->SR = FLASH_SR_EOP;
    }

out:
    FLASH->CR |= FLASH_CR_LOCK;
    IWDG->KR = CAL_IWDG_FEED;
    __set_PRIMASK(prim);
    return rc;
}

static uint32_t cal_bank_base(int bank)
{
    return (bank == 0) ? CFG_CAL_A_ADDR : CFG_CAL_B_ADDR;
}

static uint32_t cal_bank_snb(int bank)
{
    return (bank == 0) ? CFG_CAL_A_SNB : CFG_CAL_B_SNB;
}

static const cal_blob_t *cal_slot_ptr(int bank, int slot)
{
    return (const cal_blob_t *)(cal_bank_base(bank)
                                + (uint32_t)slot * CFG_CAL_SLOT_SIZE);
}

/* ---- 名义回退（§7.6）--------------------------------------------------- */
static void cal_set_nominal(void)
{
    memset(&cal_blob, 0, sizeof(cal_blob));
    cal_blob.magic = CFG_CAL_MAGIC;
    cal_blob.version = (uint16_t)CFG_CAL_BLOB_VERSION;
    cal_blob.method = 1U;
    cal_blob.flags = CAL_FLAG_STALE | CAL_FLAG_OPEN_BAD | CAL_FLAG_LOAD_BAD;
    for (uint32_t f = 0U; f < FREQ_NUM; ++f) {
        cal_blob.alpha[f][0] = 1.0;
        cal_blob.alpha[f][1] = 0.0;
        for (uint32_t r = 0U; r < RANGE_NUM; ++r) {
            cal_blob.zs[f][r][0] = (double)CFG_FALLBACK_ZS_RE;
            cal_blob.zs[f][r][1] = (double)CFG_FALLBACK_ZS_IM;
            /* k=0 / zp=0 编码"未标定"：k=0 → 用名义 k=1；zp=0 → method1 */
        }
    }
    cal_seq = 0U;
    cal_bank = -1;
    cal_slot = -1;
    cal_stale = 1;
    cal_bench = 0;
    cal_stray_pf = 0.0f;
}

/* blob(double 权威) → record(float 视图，§3.4 冻结结构) */
static void cal_blob_to_record(void)
{
    memset(&cal_rec, 0, sizeof(cal_rec));
    cal_rec.magic = cal_blob.magic;
    cal_rec.version = cal_blob.version;
    cal_rec.crc = cal_blob.crc32;
    cal_rec.built_ts = cal_blob.timestamp;
    cal_rec.build_count = (uint16_t)(cal_writes & 0xFFFFU);

    for (uint32_t f = 0U; f < FREQ_NUM; ++f) {
        cal_rec.alpha[f].alpha = cd2f(cd(cal_blob.alpha[f][0], cal_blob.alpha[f][1]));
        for (uint32_t r = 0U; r < RANGE_NUM; ++r) {
            const cdbl_t k  = cd(cal_blob.k[f][r][0],  cal_blob.k[f][r][1]);
            const cdbl_t zs = cd(cal_blob.zs[f][r][0], cal_blob.zs[f][r][1]);
            const cdbl_t zp = cd(cal_blob.zp[f][r][0], cal_blob.zp[f][r][1]);
            cal_net_t *n = &cal_rec.net[f][r];
            n->k_scale = cd2f(k);
            n->zs = cd2f(zs);
            /* zopen 由模型重建：Zopen = Zs + k·Zp（原始复数信息不丢） */
            n->zopen = cd2f(cd_add(zs, cd_mul(k, zp)));
            n->valid = (cd_mag2(zp) != 0.0) ? 1U : 0U;
            cal_rec.load[r].k_scale = n->k_scale;
            cal_rec.load[r].zs = n->zs;
            cal_rec.load[r].valid = (cd_mag2(k) != 0.0) ? 1U : 0U;
        }
    }
    /* load[] 是每档摘要，取 1 kHz 那一行（§7.1 电平系数也在 1 kHz 标定） */
    cal_rec.k_in = 0.0f;
    cal_rec.k_out = 0.0f;
}

/* record(float) → blob(double)。zp 由 zopen/zs/k 反推，保留 double 中间量。 */
static void cal_record_to_blob(cal_blob_t *b, const cal_record_t *rec, uint32_t seq)
{
    memset(b, 0, sizeof(*b));
    b->magic = CFG_CAL_MAGIC;
    b->version = (uint16_t)CFG_CAL_BLOB_VERSION;
    b->timestamp = board_millis();
    b->seq = seq;
    b->flags = (uint16_t)((cal_stale ? CAL_FLAG_STALE : 0U)
                        | (cal_bench ? CAL_FLAG_BENCH_OK : 0U));

    int any_open_bad = 0;
    int any_load_bad = 0;
    for (uint32_t f = 0U; f < FREQ_NUM; ++f) {
        b->alpha[f][0] = (double)rec->alpha[f].alpha.re;
        b->alpha[f][1] = (double)rec->alpha[f].alpha.im;
        for (uint32_t r = 0U; r < RANGE_NUM; ++r) {
            const cal_net_t *n = &rec->net[f][r];
            const cdbl_t k  = cf2d(n->k_scale);
            const cdbl_t zs = cf2d(n->zs);
            b->k[f][r][0] = k.re;   b->k[f][r][1] = k.im;
            b->zs[f][r][0] = zs.re; b->zs[f][r][1] = zs.im;
            cdbl_t zp = cd(0.0, 0.0);
            if ((n->valid != 0U) && (cd_mag2(k) != 0.0)) {
                zp = cd_div(cd_sub(cf2d(n->zopen), zs), k);
            } else if (n->valid != 0U) {
                /* 开路有效但未做双负载：k 取名义 1，Zp = Zopen − Zs */
                zp = cd_sub(cf2d(n->zopen), zs);
                b->k[f][r][0] = 1.0;
                b->k[f][r][1] = 0.0;
                any_load_bad = 1;
            } else {
                any_open_bad = 1;
                any_load_bad = 1;
            }
            b->zp[f][r][0] = zp.re;
            b->zp[f][r][1] = zp.im;
        }
    }
    if (any_open_bad != 0) { b->flags |= CAL_FLAG_OPEN_BAD; }
    if (any_load_bad != 0) { b->flags |= CAL_FLAG_LOAD_BAD; }
    b->method = (uint16_t)((any_open_bad != 0) ? 1U : 0U);
    b->pad = 0U;
    b->crc32 = cal_blob_crc(b);
}

/* ---- 启动扫描 / 装入 / 保存 --------------------------------------------- */
static int cal_slot_valid(int bank, int slot, uint32_t *seq_out)
{
    const cal_blob_t *b = cal_slot_ptr(bank, slot);
    if (b->magic != CFG_CAL_MAGIC) {
        return 0;
    }
    if (b->version != (uint16_t)CFG_CAL_BLOB_VERSION) {
        return 0;
    }
    if (cal_blob_crc(b) != b->crc32) {
        return 0;
    }
    *seq_out = b->seq;
    return 1;
}

static int cal_first_erased(int bank)
{
    for (uint32_t i = 0U; i < CFG_CAL_SLOTS; ++i) {
        const uint32_t w = *(volatile const uint32_t *)(cal_bank_base(bank)
                                                       + i * CFG_CAL_SLOT_SIZE);
        if (w == 0xFFFFFFFFU) {
            return (int)i;
        }
    }
    return -1;                 /* 85 槽写满 */
}

int cal_init(void)
{
    cal_diag = 0U;
    cal_writes = 0U;

    uint32_t best_seq = 0U;
    int best_bank = -1;
    int best_slot = -1;
    int have_valid = 0;

    for (int bank = 0; bank < 2; ++bank) {
        for (uint32_t i = 0U; i < CFG_CAL_SLOTS; ++i) {
            uint32_t seq = 0U;
            if (cal_slot_valid(bank, (int)i, &seq) != 0) {
                if ((have_valid == 0) || (seq > best_seq)) {
                    best_seq = seq;
                    best_bank = bank;
                    best_slot = (int)i;
                    have_valid = 1;
                }
            }
        }
    }

    if (have_valid == 0) {
        cal_set_nominal();
        cal_append_bank = 0;
        cal_append_slot = cal_first_erased(0);
        if (cal_append_slot < 0) {
            cal_append_slot = 0;      /* 下次保存时先擦 A */
        }
        cal_blob_to_record();
        return 1;                     /* 1 = 无有效记录，已回退名义表 */
    }

    memcpy(&cal_blob, cal_slot_ptr(best_bank, best_slot), sizeof(cal_blob));
    cal_seq = best_seq;
    cal_bank = best_bank;
    cal_slot = best_slot;
    cal_stale = ((cal_blob.flags & CAL_FLAG_STALE) != 0U) ? 1 : 0;
    cal_bench = ((cal_blob.flags & CAL_FLAG_BENCH_OK) != 0U) ? 1 : 0;
    cal_writes = best_seq;            /* seq 单调递增，兼作写入计数 */

    cal_append_bank = best_bank;
    cal_append_slot = cal_first_erased(best_bank);
    if (cal_append_slot < 0) {
        cal_append_bank = 1 - best_bank;   /* 本扇区满 → 下次擦对侧 */
        cal_append_slot = 0;
    }
    cal_blob_to_record();
    return 0;
}

int cal_load(cal_record_t *rec)
{
    if (rec == 0) {
        return -1;
    }
    *rec = cal_rec;
    return 0;
}

int cal_save(const cal_record_t *rec)
{
    if (rec == 0) {
        return -1;
    }
    cal_blob_t stage;
    cal_record_to_blob(&stage, rec, cal_seq + 1U);

    for (int attempt = 0; attempt < 2; ++attempt) {
        int bank = cal_append_bank;
        int slot = cal_append_slot;
        int need_erase = 0;

        if (slot < 0) {                       /* 本扇区满 → 切对侧，先擦后写 */
            bank = 1 - bank;
            slot = 0;
            need_erase = 1;
        } else if (cal_first_erased(bank) != slot) {
            /* 启动后状态与 Flash 实际不符（例如上次写失败）→ 重新定位 */
            slot = cal_first_erased(bank);
            if (slot < 0) {
                bank = 1 - bank;
                slot = 0;
                need_erase = 1;
            }
        }

        board_iwdg_feed();
        const int rc = cal_flash_op(cal_bank_base(bank) + (uint32_t)slot * CFG_CAL_SLOT_SIZE,
                                    cal_bank_snb(bank), need_erase,
                                    (const uint32_t *)&stage,
                                    (uint32_t)(sizeof(stage) / 4U));
        board_iwdg_feed();

        if (rc == 0) {
            uint32_t seq = 0U;
            if (cal_slot_valid(bank, slot, &seq) != 0 && seq == stage.seq) {
                memcpy(&cal_blob, &stage, sizeof(cal_blob));
                cal_seq = seq;
                cal_bank = bank;
                cal_slot = slot;
                cal_writes = seq;
                cal_append_bank = bank;
                cal_append_slot = cal_first_erased(bank);
                if (cal_append_slot < 0) {
                    cal_append_bank = 1 - bank;
                    cal_append_slot = 0;
                }
                cal_blob_to_record();
                return 0;
            }
        }
        /* 失败：该槽可能已部分编程，不能重写同一槽 → 前进到下一空白槽重试 */
        cal_diag |= DIAG_FLASH_ERR;
        cal_append_slot = cal_first_erased(bank);
        if (cal_append_slot < 0) {
            cal_append_bank = 1 - bank;
            cal_append_slot = 0;
        }
    }
    return -1;
}

int cal_erase(void)
{
    board_iwdg_feed();
    const int ra = cal_flash_op(CFG_CAL_A_ADDR, CFG_CAL_A_SNB, 1, 0, 0U);
    board_iwdg_feed();
    const int rb = cal_flash_op(CFG_CAL_B_ADDR, CFG_CAL_B_SNB, 1, 0, 0U);
    board_iwdg_feed();
    if ((ra != 0) || (rb != 0)) {
        cal_diag |= DIAG_FLASH_ERR;
        return -1;
    }
    cal_set_nominal();
    cal_append_bank = 0;
    cal_append_slot = 0;
    cal_blob_to_record();
    return 0;
}

/* ---- 纯数学：双负载闭式解 + 反解（金标准向量 G7/G8）--------------------- */
int cal_solve_two_load(double zm1_re, double zm1_im,
                       double zm2_re, double zm2_im,
                       double zopen_re, double zopen_im,
                       double rstd1, double rstd2,
                       double *k_re, double *k_im,
                       double *zs_re, double *zs_im,
                       double *zp_re, double *zp_im)
{
    const cdbl_t zm1 = cd(zm1_re, zm1_im);
    const cdbl_t zm2 = cd(zm2_re, zm2_im);
    const cdbl_t zo  = cd(zopen_re, zopen_im);
    const cdbl_t z1  = cd(rstd1, 0.0);
    const cdbl_t z2  = cd(rstd2, 0.0);

    const cdbl_t w1 = cd_sub(zm1, zo);
    const cdbl_t w2 = cd_sub(zm2, zo);
    const cdbl_t dw = cd_sub(w1, w2);

    /* 条件数 1：|W1−W2| > 1e-3·max(|W1|,|W2|) */
    const double wmax = (cd_mag(w1) > cd_mag(w2)) ? cd_mag(w1) : cd_mag(w2);
    if (cd_mag(dw) <= (double)CFG_LOAD_COND_EPS * wmax) {
        return -1;
    }

    /* ① Zp = (W2·Zstd2 − W1·Zstd1)/(W1 − W2) */
    const cdbl_t zp = cd_div(cd_sub(cd_mul(w2, z2), cd_mul(w1, z1)), dw);
    const double zp_mag = cd_mag(zp);

    /* 条件数 4：|Zp| > 100·max(|Z1|,|Z2|) → 杂散可忽略，按 method1（Zp=∞） */
    const double zmax = (rstd1 > rstd2) ? rstd1 : rstd2;
    if (zp_mag > (double)CFG_LOAD_ZP_INF * zmax) {
        *zp_re = 0.0; *zp_im = 0.0;      /* 0 编码"Zp 无效/∞" → method1 */
        *k_re = 1.0;  *k_im = 0.0;
        *zs_re = (zm1_re + zm2_re) * 0.5;
        *zs_im = (zm1_im + zm2_im) * 0.5;
        return 1;                        /* 1 = 退化解（Zp 视为 ∞） */
    }
    if (zp_mag == 0.0) {
        return -2;
    }
    /* Zp 必须是容性（Im<0）或至少非感性；纯电阻夹具解出感性 Zp 说明数据坏了 */
    if (zp.im > 0.0) {
        return -2;
    }

    /* ② k = W1 / (−Zp²/(Zstd1+Zp)) */
    const cdbl_t a1 = cd_div(cd_mul(cd(-1.0, 0.0), cd_mul(zp, zp)), cd_add(z1, zp));
    if (cd_mag2(a1) == 0.0) {
        return -2;
    }
    const cdbl_t k = cd_div(w1, a1);
    if (cd_mag2(k) == 0.0) {
        return -2;
    }

    /* ③ Zs = mean(Zm_i − k·(Zstd_i ∥ Zp)) */
    const cdbl_t zs1 = cd_sub(zm1, cd_mul(k, cd_par(z1, zp)));
    const cdbl_t zs2 = cd_sub(zm2, cd_mul(k, cd_par(z2, zp)));
    const cdbl_t zs = cd_scale(cd_add(zs1, zs2), 0.5);

    *k_re = k.re;   *k_im = k.im;
    *zs_re = zs.re; *zs_im = zs.im;
    *zp_re = zp.re; *zp_im = zp.im;
    return 0;
}

int cal_solve_inverse(double zm_re, double zm_im,
                      double zs_re, double zs_im,
                      double k_re, double k_im,
                      double zp_re, double zp_im, int zp_valid,
                      double *zx_re, double *zx_im)
{
    const cdbl_t k = (k_re == 0.0 && k_im == 0.0) ? cd(1.0, 0.0) : cd(k_re, k_im);
    const cdbl_t x = cd_div(cd_sub(cd(zm_re, zm_im), cd(zs_re, zs_im)), k);

    if (zp_valid == 0) {
        *zx_re = x.re; *zx_im = x.im;
        return 1;                        /* method1：Zp=0 */
    }
    const cdbl_t zp = cd(zp_re, zp_im);
    const cdbl_t den = cd_sub(zp, x);
    /* 条件数 3：|Zp−X| > 0.1·|Zp|，否则放大 >100× → 拒绝，退化 method1 */
    if (cd_mag(den) <= (double)CFG_LOAD_ZP_NEAR * cd_mag(zp)) {
        *zx_re = x.re; *zx_im = x.im;
        return 2;                        /* 2 = Zp 失效，按 method1 出数并置 LOAD_INVALID */
    }
    const cdbl_t zx = cd_div(cd_mul(zp, x), den);
    *zx_re = zx.re; *zx_im = zx.im;
    return 0;
}

/* ---- 频点 / 档位 / 名义阻抗 --------------------------------------------- */
int cal_band_of(uint32_t f_hz)
{
    for (uint32_t i = 0U; i < CFG_FREQ_NUM; ++i) {
        if (cal_bands[i].f_nom == f_hz) {
            return (int)i;
        }
    }
    return -1;
}

int cal_range_legal(range_t r, uint32_t f_hz)
{
    const int b = cal_band_of(f_hz);
    if ((b < 0) || ((uint32_t)r >= (uint32_t)RANGE_NUM)) {
        return 0;
    }
    return (int)cal_legal[(uint32_t)r][b];
}

cfloat_t cal_zf_eff(uint32_t f_hz, range_t r)
{
    if ((uint32_t)r >= (uint32_t)RANGE_NUM) {
        return cf_make(0.0f, 0.0f);
    }
    const double rf = (double)cal_rf_nom[(uint32_t)r];
    const double cf = (double)cal_cf_nom[(uint32_t)r];
    /* Zf_eff = Rf/(1+j2πf·Rf·Cf)，double 算完再转 float（1 MΩ@1 kHz 时
     * 2πfRfCf ≈ 0.0138，float 也够，但统一 double 避免边角档精度损失） */
    const double w = 2.0 * 3.14159265358979323846 * (double)f_hz * rf * cf;
    const cdbl_t z = cd_div(cd(rf, 0.0), cd(1.0, w));
    return cd2f(z);
}

cfloat_t cal_raw_impedance(uint32_t f_hz, range_t r, cfloat_t ratio)
{
    const int b = cal_band_of(f_hz);
    cdbl_t alpha = cd(1.0, 0.0);
    if (b >= 0) {
        alpha = cd(cal_blob.alpha[b][0], cal_blob.alpha[b][1]);
        if (cd_mag2(alpha) == 0.0) {
            alpha = cd(1.0, 0.0);
        }
    }
    /* Zm = −Zf_eff·(m1/m2)/α  （除以 α，不是乘；负号不能丢，§6.2/D8） */
    const cdbl_t zf = cf2d(cal_zf_eff(f_hz, r));
    const cdbl_t zm = cd_div(cd_mul(cd_neg(cf2d(ratio)), zf), alpha);
    return cd2f(zm);
}

int cal_apply_impedance(uint32_t f_hz, range_t r, cfloat_t zm, cfloat_t *z_out)
{
    const int b = cal_band_of(f_hz);
    if ((b < 0) || ((uint32_t)r >= (uint32_t)RANGE_NUM) || (z_out == 0)) {
        return -1;
    }
    const cdbl_t k  = cd(cal_blob.k[b][(uint32_t)r][0],  cal_blob.k[b][(uint32_t)r][1]);
    const cdbl_t zs = cd(cal_blob.zs[b][(uint32_t)r][0], cal_blob.zs[b][(uint32_t)r][1]);
    const cdbl_t zp = cd(cal_blob.zp[b][(uint32_t)r][0], cal_blob.zp[b][(uint32_t)r][1]);
    const int zp_valid = (cd_mag2(zp) != 0.0) ? 1 : 0;

    double zx_re = 0.0;
    double zx_im = 0.0;
    const int rc = cal_solve_inverse((double)zm.re, (double)zm.im,
                                     zs.re, zs.im, k.re, k.im,
                                     zp.re, zp.im, zp_valid, &zx_re, &zx_im);
    z_out->re = (float)zx_re;
    z_out->im = (float)zx_im;
    if (rc == 1) {
        cal_diag |= DIAG_OPEN_INVALID;
    } else if (rc == 2) {
        cal_diag |= DIAG_LOAD_INVALID;
    }
    if (cd_mag2(k) == 0.0) {
        cal_diag |= DIAG_LOAD_INVALID;
    }
    return rc;
}

/* ---- 采集helper：K 块 → 块间复数比平均（§5.5）--------------------------- */
#define CAL_BLK_MAX 8U
/* 只缓存每块复比值（8 B/块 ×8 = 64 B）+ 单块谱 scratch（§8.3 RAM 预算）。 */
static cfloat_t     cal_ratio[CAL_BLK_MAX];
static dsp_result_t cal_scratch;

static int cal_acquire(uint32_t band, uint16_t n, uint16_t k,
                       cfloat_t *ratio, uint16_t *sat, float *spread,
                       float *vo_rms)
{
    acq_cfg_t cfg;
    if (adc_make_cfg(band, n, &cfg) != 0) {
        return -1;
    }
    if ((k == 0U) || (k > CAL_BLK_MAX)) {
        k = 1U;
    }
    adc_clear_errors();
    uint16_t sat_sum = 0U;
    uint16_t got = 0U;
    float vo_peak_sum = 0.0f;

    for (uint16_t i = 0U; i < k; ++i) {
        static adc_block_t blk;   /* 4 KB 放 .bss，不上栈 */
        const int rc = adc_acquire(&cfg, &blk);
        if (rc != ADC_OK) {
            if (rc == ADC_ERR_AWD) { cal_diag |= DIAG_AWD_ABORT; }
            if (rc == ADC_ERR_CLIP) { cal_diag |= DIAG_CLIP; }
            if ((rc == ADC_ERR_TIMEOUT) || (rc == ADC_ERR_OVR)) { cal_diag |= DIAG_ADC_OVR; }
            if (got == 0U) {
                return -2;
            }
            break;                     /* 用已拿到的块继续，但降权 */
        }
        dsp_analyze(&blk, &cal_scratch);
        sat_sum = (uint16_t)(sat_sum + blk.sat_count);
        vo_peak_sum += cal_scratch.vo.peak;
        cal_ratio[got] = dsp_ratio(&cal_scratch);
        ++got;
    }
    if (got == 0U) {
        return -2;
    }
    float sl = 0.0f;
    float sp = 0.0f;
    *ratio = dsp_ratio_mean_arr(cal_ratio, got, &sl, &sp);
    if (sat != 0) { *sat = sat_sum; }
    if (spread != 0) { *spread = sl; }
    if (vo_rms != 0) {
        /* rms = peak·LSB/√2 */
        *vo_rms = (vo_peak_sum / (float)got) * CFG_ADC_LSB_V * 0.70710678f;
    }
    if (sat_sum > 0U) {
        cal_diag |= DIAG_CLIP;
    }
    return 0;
}

/* CAL L 是交互式台架流程：提示换标准件并等回车（无 RTC/无按键，靠串口）。 */
static int cal_wait_enter(const char *prompt, uint32_t timeout_ms)
{
    uart_write(prompt);
    const uint32_t t0 = board_millis();
    for (;;) {
        int c;
        while ((c = uart_read_byte()) >= 0) {
            if ((c == '\n') || (c == '\r')) {
                return 0;
            }
        }
        if ((board_millis() - t0) > timeout_ms) {
            return -1;
        }
        board_iwdg_feed();
    }
}

static int cal_prepare(uint32_t f_hz, range_t r)
{
    const int b = cal_band_of(f_hz);
    if (b < 0) {
        return -1;
    }
    if (cal_range_legal(r, f_hz) == 0) {
        return -1;
    }
    if (board_clock_degraded != 0U) {
        return -3;                     /* HSE 失败 → 拒绝校准（§4.1） */
    }
    if (dds_set_freq_hz(f_hz) < 0) {
        return -2;
    }
    if (relay_select_range(r) < 0) {
        return -1;
    }
    dds_hold_reset(0);
    board_delay_ms(CFG_CAL_SETTLE_MS);
    return b;
}

/* ---- α 通道失配（§7.2）-------------------------------------------------- */
int cal_measure_alpha(uint32_t f_hz, cal_alpha_t *out)
{
    if (out == 0) {
        return -1;
    }
    const int b = cal_prepare(f_hz, RANGE_10K);
    if (b < 0) {
        return -1;
    }
    relay_cal_enable(1);               /* PD6：两路调理切到同一信号 */
    board_delay_ms(CFG_CAL_SETTLE_MS);

    cfloat_t ratio = cf_make(0.0f, 0.0f);
    uint16_t sat = 0U;
    float spread = 0.0f;
    const int rc = cal_acquire((uint32_t)b, cal_bands[b].n_fine, CFG_CAL_BLOCKS,
                               &ratio, &sat, &spread, 0);
    relay_cal_enable(0);
    dds_hold_reset(1);
    if (rc != 0) {
        return -2;
    }
    if (sat > 0U) {
        return -4;                     /* CAL 通路削顶 → 数据不可用 */
    }
    /* 有效性：|α|≈1（±2%）、|arg α|<1°。超差说明 CAL 开关/AFE 有问题。 */
    const float mag = cf_mag(ratio);
    const float ph = cf_arg_deg(ratio);
    if ((fabsf(mag - 1.0f) > CFG_ALPHA_GAIN_TOL) || (fabsf(ph) > CFG_ALPHA_PH_TOL)) {
        return -3;
    }
    out->alpha = ratio;
    cal_rec.alpha[b].alpha = ratio;
    cal_blob.alpha[b][0] = (double)ratio.re;
    cal_blob.alpha[b][1] = (double)ratio.im;
    return 0;
}

/* ---- 开路（§7.3）-------------------------------------------------------- */
static float cal_stray_from_zp(cdbl_t zp, uint32_t f_hz)
{
    /* Zp ≈ 1/(jωC) → C = −1/(ω·Im(Zp))，Im<0 时为正值 */
    if (zp.im >= 0.0) {
        return 0.0f;
    }
    const double w = 2.0 * 3.14159265358979323846 * (double)f_hz;
    return (float)(-1.0 / (w * zp.im) * 1e12);   /* pF */
}

int cal_measure_open(uint32_t f_hz, range_t r, cal_net_t *out)
{
    if (out == 0) {
        return -1;
    }
    const int b = cal_prepare(f_hz, r);
    if (b < 0) {
        return (b == -3) ? -3 : -1;
    }
    cfloat_t ratio = cf_make(0.0f, 0.0f);
    uint16_t sat = 0U;
    float spread = 0.0f;
    float vo_rms = 0.0f;
    const int rc = cal_acquire((uint32_t)b, cal_bands[b].n_fine, CFG_CAL_REPEAT,
                               &ratio, &sat, &spread, &vo_rms);
    dds_hold_reset(1);
    if (rc != 0) {
        return -2;
    }

    const cfloat_t zm = cal_raw_impedance(f_hz, r, ratio);
    const float zmag = cf_mag(zm);
    const float rho = zmag / cal_rf_nom[(uint32_t)r];

    cal_net_t n;
    memset(&n, 0, sizeof(n));
    n.zopen = zm;
    n.zs = cf_make(CFG_FALLBACK_ZS_RE, CFG_FALLBACK_ZS_IM);
    n.k_scale = cf_make(0.0f, 0.0f);   /* 0 = 尚未做双负载 */

    /* 有效性两条件并列（§7.3）：ρ∈[1,100] 且 |Vo|≥1 mVrms */
    if ((rho >= CFG_OPEN_RHO_LO) && (rho <= CFG_OPEN_RHO_HI)
        && (vo_rms >= CFG_VO_RMS_MIN) && (sat == 0U)
        && (spread < CFG_SPREAD_WARN)) {
        n.valid = 1U;
    } else {
        n.valid = 0U;
        cal_diag |= DIAG_OPEN_INVALID;
    }
    *out = n;
    cal_rec.net[b][(uint32_t)r] = n;

    /* 夹具不变性指纹：10 kHz/10 kΩ 组合的等效杂散电容（§7.3、开机自检用） */
    if ((f_hz == cal_bands[CFG_BAND_10KHZ].f_nom) && (r == RANGE_10K) && (n.valid != 0U)) {
        const cdbl_t zp = cd_sub(cf2d(n.zopen), cf2d(n.zs));
        const float pf = cal_stray_from_zp(zp, f_hz);
        if ((cal_stray_pf > 0.0f) && (fabsf(pf - cal_stray_pf) > CFG_CAL_STALE_PF)) {
            cal_stale = 1;
            cal_bench = 0;
            cal_diag |= DIAG_CAL_STALE;
        }
        cal_stray_pf = pf;
    }
    return (n.valid != 0U) ? 0 : 1;    /* 1 = 采到了但判为无效（method1） */
}

/* ---- 双负载两点法（§7.4，交互式）--------------------------------------- */
static int cal_load_point(uint32_t f_hz, range_t r, float rstd,
                          cfloat_t *zm, uint16_t *sat, float *spread)
{
    const int b = cal_band_of(f_hz);
    if (b < 0) {
        return -1;
    }
    /* 入解硬规则 1：ρ = Rf/|Zstd| ∈ [0.1,1] */
    const float rho = cal_rf_nom[(uint32_t)r] / rstd;
    if ((rho < 0.1f) || (rho > 1.0f)) {
        return -3;
    }
    if (dds_set_freq_hz(f_hz) < 0) {
        return -2;
    }
    dds_hold_reset(0);
    board_delay_ms(CFG_CAL_SETTLE_MS);

    cfloat_t ratio = cf_make(0.0f, 0.0f);
    float vo = 0.0f;
    const int rc = cal_acquire((uint32_t)b, cal_bands[b].n_fine, CFG_CAL_REPEAT,
                               &ratio, sat, spread, &vo);
    dds_hold_reset(1);
    if (rc != 0) {
        return -2;
    }
    /* 入解硬规则 2/3：clip==0、spread<0.5% */
    if (*sat > 0U) {
        return -4;
    }
    if (*spread >= CFG_SPREAD_WARN) {
        return -5;
    }
    *zm = cal_raw_impedance(f_hz, r, ratio);
    return 0;
}

int cal_measure_load(uint32_t f_hz, range_t r, float rstd1, float rstd2, cal_load_t *out)
{
    if (out == 0) {
        return -1;
    }
    const int b = cal_prepare(f_hz, r);
    if (b < 0) {
        return (b == -3) ? -3 : -1;
    }
    const cal_net_t *open = &cal_rec.net[b][(uint32_t)r];
    if (open->valid == 0U) {
        return -6;                     /* 必须先 CAL O（Zopen 是方程的一部分） */
    }

    cfloat_t zm1 = cf_make(0.0f, 0.0f);
    cfloat_t zm2 = cf_make(0.0f, 0.0f);
    uint16_t sat1 = 0U;
    uint16_t sat2 = 0U;
    float sp1 = 0.0f;
    float sp2 = 0.0f;

    uart_printf("# CAL L %lu Hz %s : 标准件 1 = %ld ohm\r\n",
                (unsigned long)f_hz, relay_range_name(r), (long)rstd1);
    if (cal_wait_enter("# 插入标准件 1，稳定后按回车 > ", 60000U) != 0) {
        return -7;
    }
    int rc = cal_load_point(f_hz, r, rstd1, &zm1, &sat1, &sp1);
    if (rc != 0) {
        return rc;
    }

    uart_printf("# 标准件 2 = %ld ohm\r\n", (long)rstd2);
    if (cal_wait_enter("# 换标准件 2，稳定后按回车 > ", 60000U) != 0) {
        return -7;
    }
    rc = cal_load_point(f_hz, r, rstd2, &zm2, &sat2, &sp2);
    if (rc != 0) {
        return rc;
    }
    double k_re = 0.0, k_im = 0.0, zs_re = 0.0, zs_im = 0.0, zp_re = 0.0, zp_im = 0.0;
    const int solve = cal_solve_two_load((double)zm1.re, (double)zm1.im,
                                         (double)zm2.re, (double)zm2.im,
                                         (double)open->zopen.re, (double)open->zopen.im,
                                         (double)rstd1, (double)rstd2,
                                         &k_re, &k_im, &zs_re, &zs_im, &zp_re, &zp_im);
    if (solve < 0) {
        cal_diag |= DIAG_LOAD_INVALID;
        return -8;                     /* 病态/非物理 → 拒绝入解 */
    }

    cal_net_t *n = &cal_rec.net[b][(uint32_t)r];
    n->zload1 = zm1;
    n->zload2 = zm2;
    n->k_scale = cd2f(cd(k_re, k_im));
    n->zs = cd2f(cd(zs_re, zs_im));
    /* zopen 保持实测原始复数；Zp 由 (Zopen−Zs)/k 在 blob 里以 double 保存 */
    cal_blob.k[b][(uint32_t)r][0] = k_re;
    cal_blob.k[b][(uint32_t)r][1] = k_im;
    cal_blob.zs[b][(uint32_t)r][0] = zs_re;
    cal_blob.zs[b][(uint32_t)r][1] = zs_im;
    cal_blob.zp[b][(uint32_t)r][0] = zp_re;
    cal_blob.zp[b][(uint32_t)r][1] = zp_im;

    out->k_scale = n->k_scale;
    out->zs = n->zs;
    out->valid = (solve == 0) ? 1U : 0U;
    cal_rec.load[(uint32_t)r] = *out;
    if (solve != 0) {
        cal_diag |= DIAG_LOAD_INVALID;
        return 1;                      /* 退化解（Zp=∞） */
    }
    return 0;
}

/* ---- 电平系数 k_in/k_out（§7.1，只影响显示/诊断）------------------------ */
int cal_measure_level(float *k_in, float *k_out)
{
    if ((k_in == 0) || (k_out == 0)) {
        return -1;
    }
    const int b = cal_prepare(cal_bands[CFG_BAND_1KHZ].f_nom, RANGE_10K);
    if (b < 0) {
        return -1;
    }
    relay_cal_enable(1);               /* 两路同接 0.5 Vrms 标准电平 */
    board_delay_ms(CFG_CAL_SETTLE_MS);
    acq_cfg_t cfg;
    if (adc_make_cfg((uint32_t)b, cal_bands[b].n_fine, &cfg) != 0) {
        relay_cal_enable(0);
        dds_hold_reset(1);
        return -2;
    }
    static adc_block_t blk;   /* 4 KB 放 .bss，不上栈 */
    const int rc = adc_acquire(&cfg, &blk);
    relay_cal_enable(0);
    dds_hold_reset(1);
    if (rc != ADC_OK) {
        return -2;
    }
    dsp_result_t res;
    dsp_analyze(&blk, &res);
    if ((res.vin.peak < 1.0f) || (res.vo.peak < 1.0f)) {
        return -3;
    }
    /* TP1 名义 0.5 Vrms（VR1 机械校准，±10%）→ 码值→Vrms 系数 */
    *k_in = 0.5f / res.vin.peak;
    *k_out = 0.5f / res.vo.peak;
    cal_rec.k_in = *k_in;
    cal_rec.k_out = *k_out;
    return 0;
}

/* ---- 检查件复核（§7.4 验证：误差应 <0.1%）------------------------------- */
int cal_check_std(uint32_t f_hz, range_t r, float rstd, float *err_pct)
{
    if (err_pct == 0) {
        return -1;
    }
    const int b = cal_prepare(f_hz, r);
    if (b < 0) {
        return -1;
    }
    cfloat_t ratio = cf_make(0.0f, 0.0f);
    uint16_t sat = 0U;
    float spread = 0.0f;
    const int rc = cal_acquire((uint32_t)b, cal_bands[b].n_fine, CFG_CAL_REPEAT,
                               &ratio, &sat, &spread, 0);
    dds_hold_reset(1);
    if (rc != 0) {
        return -2;
    }
    const cfloat_t zm = cal_raw_impedance(f_hz, r, ratio);
    cfloat_t zx = cf_make(0.0f, 0.0f);
    (void)cal_apply_impedance(f_hz, r, zm, &zx);
    const float mag = cf_mag(zx);
    *err_pct = (mag - rstd) / rstd * 100.0f;
    return 0;
}

/* ---- 等级 / 状态 -------------------------------------------------------- */
int cal_get_level(void)
{
    int alpha_ok = 1;
    for (uint32_t f = 0U; f < FREQ_NUM; ++f) {
        const cfloat_t a = cal_rec.alpha[f].alpha;
        if ((a.re == 0.0f) && (a.im == 0.0f)) {
            alpha_ok = 0;
        }
    }
    if (alpha_ok == 0) {
        return 0;
    }
    int open_all = 1;
    int load_all = 1;
    int open_any = 0;
    int load_any = 0;
    for (uint32_t f = 0U; f < FREQ_NUM; ++f) {
        for (uint32_t r = 0U; r < RANGE_NUM; ++r) {
            if (cal_legal[r][f] == 0U) {
                continue;              /* 禁止组合不参与等级判定 */
            }
            const int ov = (int)cal_rec.net[f][r].valid;
            const int lv = (cal_blob.k[f][r][0] != 0.0 || cal_blob.k[f][r][1] != 0.0) ? 1 : 0;
            if (ov != 0) { open_any = 1; } else { open_all = 0; }
            if (lv != 0) { load_any = 1; } else { load_all = 0; }
        }
    }
    if ((open_all == 0) || (open_any == 0)) {
        return 1;
    }
    if ((load_all == 0) || (load_any == 0)) {
        return 2;
    }
    if ((cal_bench != 0) && (cal_stale == 0)) {
        return 4;
    }
    return 3;
}

const cal_record_t *cal_record(void) { return &cal_rec; }
int  cal_is_stale(void) { return cal_stale; }
void cal_set_stale(int stale)
{
    cal_stale = (stale != 0) ? 1 : 0;
    if (cal_stale != 0) {
        cal_bench = 0;
    }
}
void cal_set_bench_ok(int ok)
{
    cal_bench = (ok != 0) ? 1 : 0;
    if (cal_bench != 0) {
        cal_stale = 0;
    }
}
int cal_bench_ok(void) { return cal_bench; }

uint32_t cal_flash_seq(void) { return cal_seq; }
int cal_flash_slot(void) { return cal_slot; }
int cal_flash_bank(void) { return cal_bank; }
uint32_t cal_flash_crc(void) { return cal_blob.crc32; }
uint32_t cal_flash_writes(void) { return cal_writes; }

/* 供 meas/selftest 取走本模块累计的诊断位（取走后清零）。 */
uint32_t cal_take_diag(void)
{
    const uint32_t d = cal_diag;
    cal_diag = 0U;
    return d;
}

/* 标准负载表（proto 的 CAL L 默认值用）。 */
const float *cal_load_std_table(range_t r)
{
    if ((uint32_t)r >= (uint32_t)RANGE_NUM) {
        return 0;
    }
    return cal_load_std[(uint32_t)r];
}