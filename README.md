# Infra Arcana 简体中文 · Bazzite

基于官方稳定版 **v23.0.0**（2025-05-18），上游提交 `51443407f4017754e59763f12f2c7ab4704e8ee2`。

- 官方下载：https://sites.google.com/site/infraarcana/downloads
- 上游：https://gitlab.com/martin-tornqvist/ia
- 游戏许可：AGPL-3.0-or-later，原始许可证见 `source/installed_files/LICENSE.txt`。
- 全部游戏译文由 GPT-6 Luna 子代理产出；术语、动态片段与源码位置记录在 `translations/catalog.json`。

## 已安装位置

Bazzite：`~/Games/infra-arcana-zh/game/launch.sh`。Steam 库入口：**Infra Arcana 中文版**。
独立存档与设置：`~/.local/share/infra-arcana-zh/23.0.0/`。
Steam 快捷方式修改前备份：`~/Games/infra-arcana-zh/installation-records/`。

## 显示

原生 SDL2 Linux 程序。启动脚本设置 `SDL_ENABLE_SCREEN_KEYBOARD=0`，避免原版每次等待按键时自动唤起 Steam 虚拟键盘；需要时仍可使用 Steam 的手动键盘快捷键。默认使用 6×12 ASCII / 12×12 中文像素字形与最近邻整数缩放；4K 默认 4 倍缩放，地图 80×80 像素。侧栏加宽以容纳中文。全屏按原生显示尺寸创建窗口，中文换行、居中、右对齐和输入退格按 UTF-8 字符处理。

字体使用 Fusion Pixel 与 Noto；字体许可证随游戏位于 `licenses/fonts/`。
Steam 素材包含横封面、竖封面、Hero、透明 Logo 和图标。素材来源、生成提示词与散列见 `artwork/sources.json`、`artwork/prompts.json`。

## 复现

本仓库保存完整源码、素材、翻译词库、应用工具与验证记录。标签 `upstream-v23.0.0` 为原版，`unicode-baseline` 为翻译前 Unicode 支持版本。

```sh
python3 tools/apply_catalog.py
sh scripts/build-bazzite.sh
```

构建脚本默认通过 SSH 别名 `bazzite` 使用专用 Podman 容器 `infra-arcana-zh-builder`。未修改 Bazzite 系统软件包。运行时附带 SDL2_image / SDL2_mixer 库。
Steam 安装器 `tools/install_steam.py` 在 Steam 退出后执行，保持其他快捷方式不变；本次目标账号为 `146975941`。

## 覆盖率与验证边界

覆盖率应结合源码提取、位置分类、字形检查和实际游戏检查判断。`verification/apply-catalog.json` 记录应用次数；词库中保留原文的伪咒语、专名、字母快捷键及机器解析键有明确分类。文本词库无空译文不代表穷举运行中的全部随机情境；残留审计记录随交付更新。
