# TODO5: 测试用例设计

## 任务概述

设计完整的测试用例，包括单元测试、协议测试、集成测试和错误处理测试。

## 背景

为确保 OTA 库的质量和可靠性，需要设计全面的测试用例来验证所有功能。

## 参考资料

- [examples/win_sim/TESTLIST.md](examples/win_sim/TESTLIST.md) - 现有测试清单
- [smota/smota_core/](smota/smota_core/) - 核心代码
- [examples/win_sim/](examples/win_sim/) - 测试环境

## 详细要求

### 1. 测试框架设计

```c
/**
 * @file  test_framework.h
 * @brief 轻量级测试框架
 */

/* 测试断言宏 */
#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("FAIL: %s:%d - Expected %d, got %d\n", \
                   __FILE__, __LINE__, (int)(expected), (int)(actual)); \
            return -1; \
        } \
    } while(0)

#define TEST_ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL: %s:%d - Condition false\n", __FILE__, __LINE__); \
            return -1; \
        } \
    } while(0)

#define TEST_ASSERT_NULL(ptr) \
    TEST_ASSERT_EQUAL(NULL, (ptr))

#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            printf("FAIL: %s:%d - Pointer is NULL\n", __FILE__, __LINE__); \
            return -1; \
        } \
    } while(0)

/* 测试用例宏 */
#define TEST_CASE(name) \
    int test_##name(void)

#define RUN_TEST(name) \
    do { \
        printf("Running %s... ", #name); \
        if (test_##name() == 0) { \
            printf("PASS\n"); \
            g_tests_passed++; \
        } else { \
            printf("FAIL\n"); \
            g_tests_failed++; \
        } \
        g_tests_total++; \
    } while(0)

/* 测试套件结构 */
typedef int (*test_func_t)(void);

struct test_suite {
    const char      *name;
    test_func_t     *tests;
    uint16_t        count;
};
```

### 2. 单元测试用例

#### 2.1 CRC 测试 (test_crc.c)

```c
TEST_CASE(crc16_compute_simple) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint16_t crc = smota_crc16_compute(data, sizeof(data));
    TEST_ASSERT_EQUAL(0x????, crc);  /* 替换为预期值 */
    return 0;
}

TEST_CASE(crc16_verify_valid) {
    uint8_t frame[] = {/* 完整的有效帧 */};
    int ret = smota_crc16_verify(frame, sizeof(frame) - 2);
    TEST_ASSERT_EQUAL(0, ret);
    return 0;
}

TEST_CASE(crc16_verify_invalid) {
    uint8_t frame[] = {/* 被篡改的帧 */};
    int ret = smota_crc16_verify(frame, sizeof(frame) - 2);
    TEST_ASSERT_NOT_EQUAL(0, ret);
    return 0;
}
```

#### 2.2 SHA256 测试 (test_sha256.c)

```c
TEST_CASE(sha256_known_vector) {
    uint8_t data[] = "abc";
    uint8_t hash[32];
    uint8_t expected[] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };

    tc_sha256_hash(hash, data, sizeof(data) - 1);
    TEST_ASSERT_EQUAL(0, memcmp(hash, expected, 32));
    return 0;
}

TEST_CASE(sha256_empty) {
    uint8_t hash[32];
    uint8_t expected[] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
        0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };

    tc_sha256_hash(hash, NULL, 0);
    TEST_ASSERT_EQUAL(0, memcmp(hash, expected, 32));
    return 0;
}
```

#### 2.3 版本比较测试 (test_version.c)

```c
TEST_CASE(version_compare_major) {
    struct smota_version v1 = {2, 0, 0};
    struct smota_version v2 = {1, 9, 9};
    int ret = smota_version_compare(&v1, &v2);
    TEST_ASSERT_TRUE(ret > 0);
    return 0;
}

TEST_CASE(version_compare_minor) {
    struct smota_version v1 = {1, 2, 0};
    struct smota_version v2 = {1, 1, 9};
    int ret = smota_version_compare(&v1, &v2);
    TEST_ASSERT_TRUE(ret > 0);
    return 0;
}

TEST_CASE(version_compare_equal) {
    struct smota_version v1 = {1, 0, 0};
    struct smota_version v2 = {1, 0, 0};
    int ret = smota_version_compare(&v1, &v2);
    TEST_ASSERT_EQUAL(0, ret);
    return 0;
}

TEST_CASE(version_rollback_detect) {
    struct smota_version v_old = {1, 2, 0};
    struct smota_version v_new = {1, 1, 0};
    int ret = smota_version_check_rollback(&v_old, &v_new);
    TEST_ASSERT_EQUAL(-1, ret);  /* 应拒绝回滚 */
    return 0;
}
```

#### 2.4 状态机测试 (test_state.c)

```c
TEST_CASE(state_init) {
    smota_state_reset();
    smota_state_t state = smota_state_get();
    TEST_ASSERT_EQUAL(SMOTA_STATE_IDLE, state);
    return 0;
}

TEST_CASE(state_transition_valid) {
    smota_state_reset();
    smota_err_t ret = smota_state_set(SMOTA_STATE_HANDSHAKE);
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, ret);
    TEST_ASSERT_EQUAL(SMOTA_STATE_HANDSHAKE, smota_state_get());
    return 0;
}

TEST_CASE(state_transition_invalid) {
    smota_state_reset();
    smota_err_t ret = smota_state_set(SMOTA_STATE_TRANSFER);
    TEST_ASSERT_NOT_EQUAL(SMOTA_ERR_OK, ret);
    TEST_ASSERT_EQUAL(SMOTA_STATE_ERROR, smota_state_get());
    return 0;
}

TEST_CASE(state_all_transitions) {
    /* IDLE -> HANDSHAKE -> HEADER_INFO -> TRANSFER ->
     * COMPLETE -> INSTALL -> ACTIVATE -> IDLE */
    smota_state_reset();
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, smota_state_set(SMOTA_STATE_HANDSHAKE));
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, smota_state_set(SMOTA_STATE_HEADER_INFO));
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, smota_state_set(SMOTA_STATE_TRANSFER));
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, smota_state_set(SMOTA_STATE_COMPLETE));
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, smota_state_set(SMOTA_STATE_INSTALL));
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, smota_state_set(SMOTA_STATE_ACTIVATE));
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, smota_state_set(SMOTA_STATE_IDLE));
    return 0;
}
```

### 3. 协议测试用例

#### 3.1 帧解析测试 (test_frame.c)

```c
TEST_CASE(frame_parse_valid) {
    uint8_t frame[] = {
        /* Frame header */
        's', 'm', 'O', 'T', 'A',  /* SOF */
        0x01,                      /* Version */
        0x00,                      /* Flags */
        0x00,                      /* Seq */
        0x01,                      /* Cmd: HANDSHAKE */
        0x10, 0x00,                /* Length: 16 */
        /* Payload */
        0x00, 0x01, 0x02, /* ... */
        /* CRC */
        0x??, 0x??
    };
    struct smota_frame parsed;
    int ret = smota_frame_parse(frame, sizeof(frame), &parsed);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(0x01, parsed.header.cmd);
    return 0;
}

TEST_CASE(frame_build_parse_roundtrip) {
    uint8_t payload[16] = {0};
    uint8_t buffer[256];
    struct smota_frame parsed;

    int len = smota_frame_build(0x01, payload, sizeof(payload),
                                buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(len > 0);

    int ret = smota_frame_parse(buffer, len, &parsed);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(0x01, parsed.header.cmd);
    TEST_ASSERT_EQUAL(sizeof(payload), parsed.header.length);
    return 0;
}
```

### 4. 集成测试用例

#### 4.1 完整 OTA 流程测试 (test_integration.c)

```c
TEST_CASE(full_ota_flow_small) {
    /* 1. 初始化 */
    int ret = smota_init();
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, ret);

    /* 2. 握手 */
    struct smota_handshake_req hs_req = {/* ... */};
    struct smota_handshake_resp hs_resp;
    ret = smota_handle_handshake_req(&hs_req, &hs_resp);
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, ret);
    TEST_ASSERT_EQUAL(0, hs_resp.error_code);

    /* 3. 头部信息 */
    struct smota_header_info_req hi_req = {/* ... */};
    struct smota_header_info_resp hi_resp;
    ret = smota_handle_header_info_req(&hi_req, &hi_resp);
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, ret);

    /* 4. 数据传输 */
    struct smota_data_block_req db_req = {/* ... */};
    struct smota_data_block_resp db_resp;
    ret = smota_handle_data_block_req(&db_req, data, &db_resp);
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, ret);

    /* 5. 传输完成 */
    struct smota_transfer_complete_req tc_req = {/* ... */};
    struct smota_transfer_complete_resp tc_resp;
    ret = smota_handle_transfer_complete_req(&tc_req, &tc_resp);
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, ret);

    /* 6. 安装 */
    struct smota_install_req inst_req = {/* ... */};
    struct smota_install_resp inst_resp;
    ret = smota_handle_install_req(&inst_req, &inst_resp);
    TEST_ASSERT_EQUAL(SMOTA_ERR_OK, ret);

    return 0;
}
```

### 5. Mock 组件

```c
/**
 * @file  mock_comm.c
 * @brief 通信 Mock 实现
 */

static uint8_t g_mock_rx_buffer[2048];
static uint32_t g_mock_rx_len = 0;

void mock_comm_set_rx_data(const uint8_t *data, uint32_t len) {
    if (len > sizeof(g_mock_rx_buffer)) {
        len = sizeof(g_mock_rx_buffer);
    }
    memcpy(g_mock_rx_buffer, data, len);
    g_mock_rx_len = len;
}

int mock_comm_receive(uint8_t *data, uint32_t size, uint32_t timeout) {
    if (g_mock_rx_len == 0) {
        return 0;
    }
    uint32_t copy_len = (size < g_mock_rx_len) ? size : g_mock_rx_len;
    memcpy(data, g_mock_rx_buffer, copy_len);
    g_mock_rx_len = 0;
    return (int)copy_len;
}
```

### 6. 测试执行结构

```
examples/win_sim/test/
├── test_framework.h           # 测试框架
├── test_runner.c              # 测试入口
├── cases/
│   ├── test_crc.c            # CRC 测试
│   ├── test_sha256.c         # SHA256 测试
│   ├── test_version.c        # 版本测试
│   ├── test_state.c          # 状态机测试
│   ├── test_frame.c          # 帧解析测试
│   ├── test_handler.c        # 协议处理测试
│   └── test_integration.c    # 集成测试
└── mock/
    ├── mock_comm.c           # 通信 Mock
    ├── mock_flash.c          # Flash Mock
    └── mock_crypto.c         # 加密 Mock
```

## 设计要求

1. **独立性**: 每个测试用例独立运行，不依赖其他用例
2. **可重复**: 测试结果可重复，不依赖外部环境
3. **自动化**: 测试可自动执行并报告结果
4. **低耦合**: 使用 Mock 隔离外部依赖

## 验证标准

1. 所有单元测试通过
2. 所有协议测试通过
3. 所有集成测试通过
4. 代码覆盖率达标

## 输出文件

1. 创建 [examples/win_sim/test/test_framework.h](examples/win_sim/test/test_framework.h) - 测试框架
2. 创建 [examples/win_sim/test/test_runner.c](examples/win_sim/test/test_runner.c) - 测试入口
3. 创建 [examples/win_sim/test/cases/](examples/win_sim/test/cases/) - 测试用例
4. 创建 [examples/win_sim/test/mock/](examples/win_sim/test/mock/) - Mock 组件
5. 更新 [examples/win_sim/CMakeLists.txt](examples/win_sim/CMakeLists.txt) - 添加测试构建目标

---

**创建日期**: 2026-03-03
**分配者**: Manager
**执行者**: Architect
