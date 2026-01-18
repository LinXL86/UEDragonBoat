# 三消系统UI开发说明文档

> **面向对象**：客户端UI开发人员  
> **版本**：1.0  
> **引擎版本**：Unreal Engine 5.4

---

## ?? 目录

1. [系统概述](#系统概述)
2. [核心数据结构](#核心数据结构)
3. [C++接口说明](#c接口说明)
4. [UI事件流程](#ui事件流程)
5. [完整开发流程](#完整开发流程)
6. [特殊格子系统](#特殊格子系统)
7. [常见问题](#常见问题)

---

## 系统概述

### 游戏规则

- **棋盘大小**：7x7 = 49个方块
- **方块颜色**：红、蓝、绿、黄 共4种
- **消除规则**：3个或以上相同颜色的方块连成一线即可消除
- **特殊格子**：棋盘上有3个特殊位置，消除时触发特殊效果

### 索引系统

所有位置使用**单一索引**表示（0-48），公式：

```
索引 = 行号 * 7 + 列号

示例：
(0, 0) → 0
(3, 3) → 24
(6, 6) → 48
```

**棋盘索引对照表：**
```
     列0  列1  列2  列3  列4  列5  列6
行0   0    1    2    3    4    5    6
行1   7    8    9   10   11   12   13
行2  14   15   16   17   18   19   20
行3  21 22   23   24   25   26   27
行4  28   29   30   31   32   33   34
行5  35   36   37   38   39   40   41
行6  42   43   44   45   46   47   48
```

---

## 核心数据结构

### ETileColor - 方块颜色

```cpp
enum class ETileColor : uint8
{
    Red,      // 红色
    Blue,     // 蓝色
 Green,    // 绿色
    Yellow,   // 黄色
    Empty     // 空（内部使用）
};
```

### ESlotEffectType - 特殊效果类型

```cpp
enum class ESlotEffectType : uint8
{
    None,  // 无特殊效果
    SpeedUpSelf,       // 加速自己
    SlowDownEnemy,     // 减速敌人
    MoraleBoost        // 士气提升
};
```

### FFallMove - 下落移动数据

```cpp
struct FFallMove
{
    int32 FromIndex;    // 起始索引（负数表示新生成）
    int32 ToIndex;        // 目标索引
    ETileColor Color;     // 方块颜色
    bool bIsNewTile;      // 是否是新生成的方块
};
```

### FSpecialEffectData - 特殊效果数据

```cpp
struct FSpecialEffectData
{
    ESlotEffectType EffectType;     // 效果类型
    TArray<int32> TriggerIndices;   // 触发位置索引数组
};
```

**重要**：`TriggerIndices` 包含了所有触发该效果的方块索引，UI可以在这些位置播放特效。

---

## C++接口说明

### 查询函数

#### GetColorAt
```cpp
ETileColor GetColorAt(int32 TileIndex) const;
```
**功能**：获取指定索引位置的方块颜色  
**参数**：`TileIndex` - 方块索引（0-48）  
**返回**：方块颜色枚举  
**用途**：初始化、刷新方块时获取颜色

#### GetSpecialTileType
```cpp
ESlotEffectType GetSpecialTileType(int32 Index) const;
```
**功能**：获取指定索引位置的特殊格子类型  
**参数**：`Index` - 方块索引（0-48）  
**返回**：特殊效果类型（None表示普通格子）  
**用途**：初始化时判断是否需要添加特殊标识

### 输入函数

#### HandleTileInput
```cpp
bool HandleTileInput(int32 TileIndex);
```
**功能**：处理方块点击  
**参数**：`TileIndex` - 被点击的方块索引  
**返回**：是否成功处理  
**用途**：玩家点击方块时调用

### 状态推进函数

#### AdvanceGameState
```cpp
void AdvanceGameState();
```
**功能**：推进游戏状态  
**用途**：**每个动画完成后必须调用**，让游戏进入下一阶段

### 辅助转换函数

#### IndexToRowCol
```cpp
void IndexToRowCol(int32 Index, int32& OutRow, int32& OutCol) const;
```
**功能**：将索引转换为行列坐标  
**用途**：布局Widget时需要行列位置

#### RowColToIndex
```cpp
int32 RowColToIndex(int32 Row, int32 Col) const;
```
**功能**：将行列坐标转换为索引  
**用途**：从Grid Panel位置计算索引

---

## UI事件流程

### ?? 事件总览

UI需要实现以下5个蓝图事件：

| 事件名 | 触发时机 | 动画完成后是否需要调用AdvanceGameState |
|--------|---------|---------------------------------------|
| OnBoardInitialized | 游戏开始 | ? 否 |
| OnSwapAnimTriggered | 交换方块 | ? 是 |
| OnMatchesCleared | 消除匹配 | ? 是 |
| OnFallAnimTriggered | 下落填充 | ? 是 |
| OnBoardReshuffle | 死锁洗牌 | ? 否（自动进入Idle）|

---

### ?? 事件1：OnBoardInitialized

**触发时机**：游戏初始化完成，棋盘已生成

**参数**：无

**UI任务**：
1. 创建49个方块Widget
2. 设置每个方块的颜色
3. 为特殊格子添加视觉标识

**蓝图实现**：
```
Event OnBoardInitialized
 ↓
For Index = 0 to 48
 ↓
 ├─ Create TileWidget
 │
 ├─ Get Color: GetColorAt(Index)
 │   └─ Set Widget Color
 │
 ├─ Get Special Type: GetSpecialTileType(Index)
 │   └─ If Type != None
 │       └─ Add Special Overlay (光晕/图标)
 │
 └─ Store Widget in Array[Index]
```

**特殊标识示例**：
- `SpeedUpSelf`：蓝色光晕 + ? 图标
- `SlowDownEnemy`：红色光晕 + ?? 图标
- `MoraleBoost`：金色光晕 + ? 图标

---

### ?? 事件2：OnSwapAnimTriggered

**触发时机**：玩家交换了两个方块

**参数**：
- `IndexA`：第一个方块索引
- `IndexB`：第二个方块索引
- `bIsSuccessful`：是否有效交换

**UI任务**：
- **成功**：播放位置互换动画
- **失败**：播放晃动动画，回到原位

**蓝图实现**：
```
Event OnSwapAnimTriggered (IndexA, IndexB, bIsSuccessful)
 ↓
Branch on bIsSuccessful
 ├─ True (成功交换):
 │   └─ Play Swap Animation (0.3秒)
 │       ├─ Widget[IndexA] 移动到 IndexB 位置
 │       ├─ Widget[IndexB] 移动到 IndexA 位置
 │    └─ On Complete: Call AdvanceGameState() ← 必须调用
 │
 └─ False (失败):
     └─ Play Bounce Animation (0.3秒)
         ├─ Widget[IndexA] 向 IndexB 晃动后回位
       ├─ Widget[IndexB] 向 IndexA 晃动后回位
         └─ On Complete: Call AdvanceGameState() ← 必须调用
```

?? **重要**：动画完成后必须调用 `AdvanceGameState()`

---

### ?? 事件3：OnMatchesCleared

**触发时机**：检测到匹配，需要消除

**参数**：
- `ClearedIndices`：被消除的方块索引数组
- `TriggeredEffects`：触发的特殊效果数组（可能为空）

**UI任务**：
1. 播放所有被消除方块的动画
2. 如果触发了特殊效果，在触发位置播放特效
3. 播放全屏特效（根据效果类型）

**蓝图实现**：
```
Event OnMatchesCleared (ClearedIndices, TriggeredEffects)
 ↓
Step 1: 播放消除动画
 ├─ For Each Index in ClearedIndices
 │   └─ Play Clear Animation (淡出/缩小)
 ↓
Step 2: 播放局部特效（如果有特殊格子）
 ├─ If TriggeredEffects.Num() > 0
 │   └─ For Each Effect in TriggeredEffects
 │       └─ For Each Index in Effect.TriggerIndices
 │           └─ 在 Index 位置播放爆炸特效
 ↓
Step 3: 播放全屏特效
 ├─ If TriggeredEffects.Num() > 0
 │   └─ For Each Effect in TriggeredEffects
 │       ├─ Switch on EffectType:
 │       │   ├─ SpeedUpSelf: 播放蓝色光波
 │       │   ├─ SlowDownEnemy: 播放红色震动
 ││   └─ MoraleBoost: 播放金色光柱
 │  │
 │ └─ Show Text: "{EffectType} x{TriggerIndices.Num()}!"
 ↓
Step 4: 等待动画完成
 └─ Delay 0.5秒
   └─ Call AdvanceGameState() ← 必须调用
```

**特效示例**：

| 效果类型 | 局部特效（触发位置） | 全屏特效 |
|---------|---------------------|---------|
| SpeedUpSelf | 蓝色爆炸 + 闪电粒子 | 蓝色光波从左到右 |
| SlowDownEnemy | 红色爆炸 + 减速标记 | 红色震动波 |
| MoraleBoost | 金色爆炸 + 星星粒子 | 金色光柱向上 |

?? **重要**：`TriggerIndices` 告诉你在哪些位置播放特效

---

### ?? 事件4：OnFallAnimTriggered

**触发时机**：消除后需要下落填充

**参数**：
- `FallMoves`：下落移动数组

**UI任务**：
1. 新生成的方块从顶部飞入
2. 现有方块下落到新位置
3. 为新生成的方块添加特殊标识（如果需要）

**蓝图实现**：
```
Event OnFallAnimTriggered (FallMoves)
 ↓
For Each FallMove in FallMoves
 ↓
Branch on bIsNewTile
 ├─ True (新生成):
 │   ├─ Create New Widget
 │   ├─ Set Color: FallMove.Color
 │   ├─ Play Fly-In Animation
 │   │   └─ 从顶部外侧飞入到 ToIndex 位置
 │   │
 │   └─ Check Special Tile
 │       └─ Get Special Type: GetSpecialTileType(ToIndex)
 │           └─ If Type != None
 │   └─ Add Special Overlay
 │
 └─ False (下落):
     └─ Play Fall Animation
         └─ Widget 从 FromIndex 移动到 ToIndex
 ↓
Wait All Animations Complete
 ↓
Call AdvanceGameState() ← 必须调用
```

**注意事项**：
- 新方块可能覆盖特殊格子位置，需要重新添加标识
- 建议下落速度：0.3-0.5秒
- `FromIndex` 为负数表示新生成（虚拟起始位置）

---

### ?? 事件5：OnBoardReshuffle

**触发时机**：死锁（无可用移动），需要洗牌

**参数**：无

**UI任务**：
1. 播放全局洗牌动画
2. 刷新所有方块颜色
3. **保持特殊格子标识不变**

**蓝图实现**：
```
Event OnBoardReshuffle
 ↓
Play Global Shuffle Animation (1.0秒)
 ├─ 所有方块旋转/翻转/抖动
 │
 └─ 在动画中刷新颜色
     └─ For Index = 0 to 48
 └─ Get New Color: GetColorAt(Index)
  └─ Update Widget[Index] Color
 ↓
Animation Complete
 └─ 游戏自动进入 Idle，无需调用 AdvanceGameState()
```

?? **重要**：
- 洗牌**不会改变**特殊格子位置
- **不需要**调用 `AdvanceGameState()`
- 只更新颜色，保留特殊标识

---

## 完整开发流程

### 第一步：创建Widget结构

**推荐Widget层级：**
```
WBP_Match3Grid (主容器)
 ├─ UniformGridPanel (7x7布局)
 │   └─ WBP_Tile (方块Widget, 49个)
 │    ├─ Image_Color (方块主体)
 │       ├─ Overlay_Special (特殊标识层)
 │       │   ├─ Image_Glow (光晕)
 │       │   └─ Image_Icon (图标)
 │       └─ Widget_Animation (动画层)
 │
 └─ Canvas_ScreenEffects (全屏特效层)
     ├─ Effect_SpeedUp
     ├─ Effect_SlowDown
     └─ Effect_Morale
```

**WBP_Tile 变量：**
```
int32 TileIndex;      // 存储自己的索引
UImage* ColorImage;    // 颜色图片
UWidgetAnimation* ClearAnim;  // 消除动画
UWidgetAnimation* FallAnim;   // 下落动画
```

---

### 第二步：初始化棋盘

```
Event OnBoardInitialized
 ↓
For Index = 0 to 48
 ↓
 ├─ Index To Row Col (Index) → Row, Col
 ├─ Create Widget: WBP_Tile
 ├─ Widget.TileIndex = Index
 ├─ Add to Grid at (Row, Col)
 │
 ├─ Set Color: GetColorAt(Index)
 │
 └─ Check Special: GetSpecialTileType(Index)
     └─ If Type != None
         └─ Show Special Overlay
```

---

### 第三步：处理点击

**在 WBP_Tile 中：**
```
Event OnMouseButtonDown
 ↓
Get Datamanagement Reference
 ↓
Call HandleTileInput(Self.TileIndex)
```

---

### 第四步：实现动画

#### 消除动画
```
Function PlayClearAnimation(Index)
 ↓
 Get Widget[Index]
 ↓
 Play Animation: ClearAnim
  ├─ Opacity: 1.0 → 0.0
  ├─ Scale: 1.0 → 0.5
  └─ Duration: 0.5秒
```

#### 下落动画
```
Function PlayFallAnimation(FromIndex, ToIndex)
 ↓
 Get Widget[FromIndex]
 ↓
 Get Positions: FromPos, ToPos
 ↓
 Play Animation: FallAnim
  ├─ Position: FromPos → ToPos
  ├─ EaseOut
  └─ Duration: 0.4秒
```

---

### 第五步：实现事件响应

将前面的事件蓝图实现添加到 `WBP_Match3Grid`。

---

## 特殊格子系统

### 默认配置

系统会自动配置3个特殊格子（对称分布）：

```
棋盘布局：
行0:  ··  ·  ·  ·  ·  ·
行1:  ·  ·  ·  ·  ··  ·
行2:  ·  ·  ·  ·  ·  ·  ·
行3:  ·  ? ·  ? ·  ?? ·   ← 中间行
行4:  ·  ·  ·  ·  ·  ·  ·
行5:  ·  ·  ·  ·  ·  ·  ·
行6:  ·  ·  ·  ·  ·  ·  ·
```

- **索引 22** (3, 1)：? 加速自己
- **索引 24** (3, 3)：? 士气提升
- **索引 26** (3, 5)：?? 减速敌人

### 特殊标识建议

**方案1：光晕边框**
```cpp
// 蓝图
Set Border Color:
  - SpeedUpSelf: (0, 0.5, 1, 0.8) 蓝色
  - SlowDownEnemy: (1, 0.3, 0, 0.8)    红色
  - MoraleBoost: (1, 0.8, 0, 0.8)      金色

Play Glow Animation (循环):
  - Scale: 1.0 → 1.1 → 1.0
  - Opacity: 0.6 → 1.0 → 0.6
```

**方案2：图标叠加**
- 在方块中心叠加小图标
- 可以使用材质或贴图

### 特殊效果表现

#### 触发位置效果（局部）
```
爆炸粒子系统:
  - 生命周期: 0.5秒
  - 颜色: 根据 EffectType
  - 大小: 方块的1.5倍

光波扩散:
  - 从中心向外
  - 颜色渐变
```

#### 全屏效果

**加速 (SpeedUpSelf)**
```
UMG Animation:
  - 蓝色光波从左到右扫过
  - 速度线粒子
  - 文字提示: "加速 x{Count}!"
```

**减速 (SlowDownEnemy)**
```
UMG Animation:
  - 红色震动波
  - 屏幕轻微抖动
  - 文字提示: "减速敌人 x{Count}!"
```

**士气 (MoraleBoost)**
```
UMG Animation:
  - 金色光柱从下向上
  - 星星粒子
  - 文字提示: "士气提升 x{Count}!"
```

---

## 常见问题

### Q1: 动画完成后忘记调用 AdvanceGameState() 会怎样？

**A:** 游戏会卡住，无法继续。务必在以下事件的动画结束后调用：
- OnSwapAnimTriggered ?
- OnMatchesCleared ?
- OnFallAnimTriggered ?

### Q2: 如何调试事件触发？

**A:** 在蓝图中添加 Print String：
```
Event OnMatchesCleared (ClearedIndices, TriggeredEffects)
 ↓
 Print: "Cleared {ClearedIndices.Num()} tiles"
 Print: "Triggered {TriggeredEffects.Num()} effects"
```

### Q3: 特殊格子标识消失了怎么办？

**A:** 检查以下情况：
1. **下落后**：新生成的方块需要重新添加标识
   ```
   If bIsNewTile
  └─ Get Special Type(ToIndex)
   └─ Add Overlay if needed
   ```
2. **洗牌后**：应该保持标识不变，只更新颜色

### Q4: 如何测试特殊效果？

**A:** 在编辑器中手动配置特殊格子位置，然后手动消除它们：
1. 打开 BP_Datamanagement
2. 设置 SpecialAreaGrid
3. 运行游戏，消除对应位置

### Q5: Widget数组和Grid Panel如何对应？

**A:** 推荐使用索引数组：
```
TArray<WBP_Tile*> TileWidgets;  // Size = 49

// 初始化
For Index = 0 to 48
 ├─ Create Widget
 ├─ TileWidgets[Index] = Widget
 └─ Add to Grid Panel

// 访问
Widget = TileWidgets[Index];
```

### Q6: 多个特效同时触发如何处理？

**A:** 依次播放，或并行播放：
```
// 方案1: 依次播放
For Each Effect in TriggeredEffects
 └─ Play Effect → Wait Complete → Next

// 方案2: 并行播放（推荐）
For Each Effect in TriggeredEffects
 └─ Play Effect (同时)
↓
Wait Longest Duration
 └─ AdvanceGameState()
```

---

## 性能优化建议

### Widget池

对于频繁创建/销毁的方块，使用对象池：
```
TArray<WBP_Tile*> WidgetPool;

Function GetPooledWidget()
 ├─ If Pool.Num() > 0
 │   └─ Return Pool.Pop()
 └─ Else
     └─ Return Create New Widget

Function ReturnToPool(Widget)
 └─ Pool.Add(Widget)
     └─ Set Visibility: Hidden
```

### 动画优化

- 使用 UMG Animation 而非 Tick
- 避免在 Tick 中计算位置
- 使用 Timeline 代替 Delay + Loop

---

## 附录：完整事件流程图

```
游戏开始
 ↓
OnBoardInitialized (创建UI)
 ↓
等待玩家操作
 ↓
HandleTileInput (点击方块)
 ↓
OnSwapAnimTriggered (交换动画)
 ↓
AdvanceGameState() ← UI调用
 ↓
OnMatchesCleared (消除动画 + 特效)
 ↓
AdvanceGameState() ← UI调用
 ↓
OnFallAnimTriggered (下落动画)
 ↓
AdvanceGameState() ← UI调用
 ↓
检查是否有新的匹配
 ├─ 有 → OnMatchesCleared (连消)
 └─ 无 → 检查死锁
     ├─ 死锁 → OnBoardReshuffle
└─ 正常 → 等待玩家操作
```

---

## 联系与支持

如有问题，请联系C++开发团队。

**文档版本**：1.0  
**最后更新**：2024  
**兼容引擎**：Unreal Engine 5.4
