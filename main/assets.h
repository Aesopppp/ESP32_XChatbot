/*
 * 固件资源管理：挂载资源分区、加载唤醒模型、更新显示主题与字形能力。
 * LVGL 与 Emote 使用不同资源策略；资源更新和固件 OTA 是两个独立流程。
 * 读取到的资源地址通常借用 Flash 映射，卸载或覆盖资源后不能继续访问旧地址。
 */

#ifndef ASSETS_H
#define ASSETS_H

#include <functional>
#include <memory>
#include <string>

#include <esp_partition.h>
#include <cJSON.h>
#include <model_path.h>
#include <map>
#include <string>

#if HAVE_LVGL
#include <spi_flash_mmap.h>
#endif

struct Asset {
    size_t size;
    size_t offset;
};

// 握手时公布的字形能力；bundle / 字号 / bpp 必须与服务端下发位图匹配。
struct TextFontCapability {
    bool glyph_push = false;
    std::string bundle;
    std::string charset;
    int size = 0;
    int bpp = 0;
};

class Assets {
public:
    static Assets& GetInstance() {
        static Assets instance;
        return instance;
    }
    ~Assets();

    // 将下载数据写入资源分区，进度回调报告百分比和速度；耗时任务应在合适线程执行。
    bool Download(std::string url,
                  std::function<void(int progress, size_t speed)> progress_callback);
    // 加载模型及显示资源，refresh_display_theme 控制是否立即重建当前显示主题。
    bool Apply(bool refresh_display_theme = true);
    // 成功时 ptr 借用已挂载的资源，size 按字节计；调用者不能 free，也不能跨卸载持有。
    bool GetAssetData(const std::string& name, void*& ptr, size_t& size);

    inline bool partition_valid() const { return partition_valid_; }
    inline std::string default_assets_url() const { return default_assets_url_; }
    inline TextFontCapability text_font_capability() const { return text_font_capability_; }

private:
    Assets();
    Assets(const Assets&) = delete;
    Assets& operator=(const Assets&) = delete;

    bool InitializePartition();
    void UnApplyPartition();
    static bool FindPartition(Assets* assets);
    static bool LoadSrmodelsFromIndex(Assets* assets, cJSON* root = nullptr);
    void UseBuiltInTextFontCapability();
    void DisableTextFontGlyphPush();

    // 不同显示后端对资源格式和挂载方式不同，用策略封装以保持外层调用一致。
    class AssetStrategy {
    public:
        virtual ~AssetStrategy() = default;
        virtual bool Apply(Assets* assets, bool refresh_display_theme = true) = 0;
        virtual bool InitializePartition(Assets* assets) = 0;
        virtual void UnApplyPartition(Assets* assets) = 0;
        virtual bool GetAssetData(Assets* assets, const std::string& name, void*& ptr,
                                  size_t& size) = 0;
    };

    class LvglStrategy : public AssetStrategy {
    public:
        bool Apply(Assets* assets, bool refresh_display_theme = true) override;
        bool InitializePartition(Assets* assets) override;
        void UnApplyPartition(Assets* assets) override;
        bool GetAssetData(Assets* assets, const std::string& name, void*& ptr,
                          size_t& size) override;

    private:
        static uint32_t CalculateChecksum(const char* data, uint32_t length);
        std::map<std::string, Asset> assets_;
        esp_partition_mmap_handle_t mmap_handle_ = 0;
        const char* mmap_root_ = nullptr;
        bool checksum_valid_ = false;
    };

    class EmoteStrategy : public AssetStrategy {
    public:
        bool Apply(Assets* assets, bool refresh_display_theme = true) override;
        bool InitializePartition(Assets* assets) override;
        void UnApplyPartition(Assets* assets) override;
        bool GetAssetData(Assets* assets, const std::string& name, void*& ptr,
                          size_t& size) override;
    };

    // Strategy instance
    // 策略对象由 Assets 单独拥有，具体资源句柄由对应策略负责清理。
    std::unique_ptr<AssetStrategy> strategy_;

protected:
    const esp_partition_t* partition_ = nullptr;
    bool partition_valid_ = false;
    std::string default_assets_url_;
    TextFontCapability text_font_capability_;
    srmodel_list_t* models_list_ = nullptr;
};

#endif
