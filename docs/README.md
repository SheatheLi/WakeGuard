# WakeGuard · 技术交互展示

该目录提供**GitHub Pages独立静态页面**：`index.html`、统一CSS、SVG证据图及页面直接引用的JSON实验资料。它与项目根目录README承担不同职责。

- **项目README：** 软件功能、架构、运行方式、源码、验证和限制。
- **本展示页面：** 问题动机、机制对照、研究图及数据来源。页面中的动态流程是**教学示意**，不是实时系统采样或真机视频。
- **原始数据：** 打开`data/`或项目根目录`evidence/`核对实验。

## 发布方式

在GitHub对应仓库的 **Settings → Pages → Build and deployment → Deploy from a branch** 中设置 **Branch = main / Folder = /docs** 并保存。发布完成后，实际网址由GitHub Pages设置页提供；不要在README提前写入未经确认的虚构链接。建议把正式网址放入仓库 **About → Website**。

本地预览可直接用浏览器打开`docs/index.html`，或在项目根目录运行`python -m http.server 8000`后访问`http://localhost:8000/docs/`。

## 证据边界

图表和数值只在标记的样本、基线、设备/环境及版本上成立。没有真机/线上部署证据时，不得暗示其存在。
