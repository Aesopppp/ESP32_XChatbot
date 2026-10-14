/*
 * 动态字形的数据结构与专用分配器。
 * 字形包含 Unicode、字距、位图尺寸、偏移和像素；优先使用可用的 PSRAM 存储这些数据。
 * 分配器遇到长度溢出或分配失败会终止，网络入口必须先限制字形数量和位图大小。
 */

#include "text_glyph.h"

bool TextGlyphStorageUsesPsram() { return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0; }
