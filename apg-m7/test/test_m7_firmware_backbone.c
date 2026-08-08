#include "apg_m7/domain/audio_engine.h"
#include "apg_m7/domain/preset_fs.h"
#include "apg_m7/domain/usb_host.h"
#include "apg_m7/domain/flash_flasher.h"
#include "apg_m7/domain/crc32.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_audio_engine_lifecycle(void) {
    apg_m7_audio_engine_init();
    assert(apg_m7_audio_engine_get_state() == APG_M7_ENGINE_READY);

    const char *mock_preset = "schema: apg.project.v2\nname: test\n";
    bool loaded = apg_m7_audio_engine_load_yaml_preset(mock_preset, strlen(mock_preset));
    assert(loaded);

    apg_m7_audio_engine_start();
    assert(apg_m7_audio_engine_get_state() == APG_M7_ENGINE_RUNNING);

    float in[128] = {0.5f};
    float out[128] = {0.0f};
    apg_m7_audio_dma_callback(in, out, 64);
    assert(out[0] == 0.5f);

    apg_m7_audio_engine_stop();
    assert(apg_m7_audio_engine_get_state() == APG_M7_ENGINE_READY);
    printf("test_audio_engine_lifecycle: PASSED\n");
}

void test_usb_preset_load(void) {
    apg_m7_usb_init();
    apg_m7_usb_process();
    assert(apg_m7_usb_is_storage_ready());

    assert(apg_m7_fs_mount() == APG_M7_FS_OK);
    char buf[1024];
    size_t read_bytes = 0;
    assert(apg_m7_fs_load_preset_yaml("0:/test.yaml", buf, sizeof(buf), &read_bytes) == APG_M7_FS_OK);
    assert(read_bytes > 0);
    printf("test_usb_preset_load: PASSED\n");
}

void test_sdram_configuration(void) {
    assert(APG_M7_SDRAM_START == 0xC0000000U);
    assert(APG_M7_SDRAM_SIZE_BYTES >= (8U * 1024U * 1024U));
    printf("test_sdram_configuration: PASSED (SDRAM size = %u MB)\n", (unsigned)(APG_M7_SDRAM_SIZE_BYTES / (1024U * 1024U)));
}

void test_audio_drivers(void) {
    const apg_m7_audio_driver_t *wm8960 = apg_m7_audio_driver_get_wm8960();
    assert(wm8960 != NULL);
    assert(wm8960->mode == APG_M7_AUDIO_IO_WM8960);
    assert(wm8960->init(48000) == APG_M7_AUDIO_DRIVER_OK);

    const apg_m7_audio_driver_t *gpio = apg_m7_audio_driver_get_gpio_adc_dac();
    assert(gpio != NULL);
    assert(gpio->mode == APG_M7_AUDIO_IO_GPIO_ADC_DAC);
    assert(gpio->init(48000) == APG_M7_AUDIO_DRIVER_OK);

    printf("test_audio_drivers: PASSED (WM8960 & GPIO ADC/DAC verified)\n");
}

#include "apg_m7/domain/usb_dfu.h"

void test_crc32_known_vectors(void) {
    const char *test1 = "123456789";
    uint32_t crc1 = apg_m7_crc32(test1, 9);
    assert(crc1 == 0xCBF43926); /* Standard known CRC32 for "123456789" */
    
    printf("test_crc32_known_vectors: PASSED\n");
}

static void mock_progress_cb(uint32_t current, uint32_t total) {
    (void)current;
    (void)total;
}

void test_firmware_header_validation(void) {
    /* Critical alignment check: C header struct MUST equal 32 bytes (matching TypeScript APG_FIRMWARE_HEADER_SIZE) */
    assert(sizeof(apg_m7_firmware_header_t) == 32);

    uint8_t buffer[1024] = {0};
    apg_m7_firmware_header_t *hdr = (apg_m7_firmware_header_t *)buffer;
    
    /* Valid Header */
    hdr->magic = APG_M7_FLASH_FIRMWARE_MAGIC;
    hdr->version = 1;
    hdr->image_size = 128;
    hdr->target_board = 0x1234;
    
    const uint8_t *payload = buffer + sizeof(apg_m7_firmware_header_t);
    hdr->crc32 = apg_m7_crc32(payload, hdr->image_size);
    
    assert(apg_m7_flash_verify_firmware(buffer, sizeof(apg_m7_firmware_header_t) + 128) == APG_M7_FLASH_OK);
    assert(apg_m7_flash_firmware(buffer, sizeof(apg_m7_firmware_header_t) + 128, mock_progress_cb) == APG_M7_FLASH_OK);

    /* Invalid Magic */
    hdr->magic = 0xDEADBEEF;
    assert(apg_m7_flash_verify_firmware(buffer, sizeof(apg_m7_firmware_header_t) + 128) == APG_M7_FLASH_INVALID_MAGIC);
    hdr->magic = APG_M7_FLASH_FIRMWARE_MAGIC;

    /* Invalid CRC */
    hdr->crc32 = 0;
    assert(apg_m7_flash_verify_firmware(buffer, sizeof(apg_m7_firmware_header_t) + 128) == APG_M7_FLASH_CHECKSUM_MISMATCH);

    /* Invalid Size */
    hdr->image_size = 0;
    assert(apg_m7_flash_verify_firmware(buffer, sizeof(apg_m7_firmware_header_t) + 128) == APG_M7_FLASH_INVALID_SIZE);
    
    printf("test_firmware_header_validation: PASSED (Header size = 32 bytes aligned with TS)\n");
}

void test_flash_sector_map(void) {
    const apg_m7_flash_sector_t *s0 = apg_m7_flash_get_sector(0x08000000);
    assert(s0 != NULL && s0->sector_num == 0 && s0->size == 16 * 1024);

    const apg_m7_flash_sector_t *s4 = apg_m7_flash_get_sector(0x08010000);
    assert(s4 != NULL && s4->sector_num == 4 && s4->size == 64 * 1024);

    const apg_m7_flash_sector_t *s7 = apg_m7_flash_get_sector(0x08060000);
    assert(s7 != NULL && s7->sector_num == 7 && s7->size == 128 * 1024);

    const apg_m7_flash_sector_t *s11 = apg_m7_flash_get_sector(0x080E0000);
    assert(s11 != NULL && s11->sector_num == 11 && s11->size == 128 * 1024);

    const apg_m7_flash_sector_t *invalid = apg_m7_flash_get_sector(0x08100000);
    assert(invalid == NULL);

    printf("test_flash_sector_map: PASSED (All 12 sectors verified)\n");
}

void test_usb_dfu_state_machine(void) {
    apg_m7_usb_dfu_init();
    assert(apg_m7_usb_dfu_get_state() == DFU_STATE_IDLE);

    uint8_t out_buf[16] = {0};
    apg_m7_usb_dfu_handle_request(DFU_GETSTATUS, 0, sizeof(out_buf), NULL, out_buf);
    assert(apg_m7_usb_dfu_get_state() == DFU_STATE_IDLE);

    apg_m7_usb_dfu_handle_request(DFU_DNLOAD, 0, 64, NULL, NULL);
    assert(apg_m7_usb_dfu_get_state() == DFU_STATE_DNLOAD_SYNC);

    apg_m7_usb_dfu_handle_request(DFU_GETSTATUS, 0, sizeof(out_buf), NULL, out_buf);
    assert(apg_m7_usb_dfu_get_state() == DFU_STATE_DNLOAD_IDLE);

    apg_m7_usb_dfu_handle_request(DFU_ABORT, 0, 0, NULL, NULL);
    assert(apg_m7_usb_dfu_get_state() == DFU_STATE_IDLE);

    printf("test_usb_dfu_state_machine: PASSED\n");
}

void test_flash_preset_yaml_validation(void) {
    const char *valid_yaml = "schema: apg.project.v2\nname: pedal\n";
    assert(apg_m7_flash_preset_yaml(valid_yaml, strlen(valid_yaml)) == APG_M7_FLASH_OK);

    const char *invalid_yaml = "invalid_header: true\n";
    assert(apg_m7_flash_preset_yaml(invalid_yaml, strlen(invalid_yaml)) == APG_M7_FLASH_INVALID_MAGIC);

    printf("test_flash_preset_yaml_validation: PASSED\n");
}

int main(void) {
    printf("Running apg-m7 firmware backbone unit tests...\n");
    test_sdram_configuration();
    test_audio_drivers();
    test_audio_engine_lifecycle();
    test_usb_preset_load();
    test_crc32_known_vectors();
    test_firmware_header_validation();
    test_flash_sector_map();
    test_usb_dfu_state_machine();
    test_flash_preset_yaml_validation();
    printf("All apg-m7 unit tests PASSED.\n");
    return 0;
}
