图片资源目录约定

- `app/`
  - 放应用级资源，例如窗口图标、品牌 logo
  - 当前示例：`logo.png`

- `icons/`
  - 放工具栏、按钮等小图标
  - 当前示例：`play.png`、`patse.png`

- `sim/`
  - 放各测试用例的仿真图、示意图
  - 8.5 已放在 `sim/` 下：
    - `BW200k.png`
    - `BW400k.png`
    - `BW600k.png`
    - `BW800k.png`
  - 8.7 放在 `sim/8_7/` 下：
    - `schematic_0.png` ~ `schematic_7.png`

建议

- 新增测试用例图片时，优先按用例建立子目录，例如：
  - `sim/8_6/`
  - `sim/8_8/`
- 先不要随意改已有图片文件名，避免影响同事的识别和现有代码映射。
- 新增或移动图片后，需要同步更新：
  - `resources/qrc/resources.qrc`
  - 代码中的 `:/...` 资源引用路径
