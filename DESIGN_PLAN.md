# KeyStats 下一版设计方案

> 状态：待实现（由用户在本地完成）  
> 目的：防止远程直接修改导致版本混乱，由用户本地可控地实施。

---

## 一、项目结构重组

### 1.1 当前问题

根目录散落多个 `.go` 文件（`main.go`、`app.go`、`drag_windows.go`），不符合 Go 项目惯例，后续难以扩展。

### 1.2 目标结构

```
key-stats/
├── cmd/
│   └── key-stats/
│       └── main.go              # 唯一入口（embed + wails.Run）
│
├── pkg/
│   ├── app/
│   │   └── app.go               # App struct + 生命周期 + API
│   └── drag/
│       └── drag_windows.go      # Win32 窗口拖动（Windows build tag）
│
├── internal/                    # 内部实现（不导出）
│   ├── db/
│   │   └── sqlite.go
│   ├── models/
│   │   └── models.go
│   ├── service/
│   │   └── keyboard.go
│   └── stats/
│       └── stats.go
│
├── frontend/                    # Svelte 前端（保持不变）
├── build/                       # 图标、manifest（保持不变）
├── wails.json                   # 需同步更新路径引用
├── go.mod
├── go.sum
├── .gitignore
└── README.md / BLUEPRINT.md
```

### 1.3 迁移步骤

1. `main.go` → `cmd/key-stats/main.go`
2. `app.go` → `pkg/app/app.go`（package 改为 `app`，调整 import）
3. `drag_windows.go` → `pkg/drag/drag_windows.go`（package 改为 `drag`，调整 import）
4. `wails.json` 中如有路径引用需更新
5. `go.mod` module path 保持不变

---

## 二、优雅关闭 + 隐藏菜单

### 2.1 设计原则

不在界面上直接放置关闭/最小化按钮，避免破坏简洁美感。采用 **"主触发 + 次触发"** 双轨方案。

### 2.2 布局方案

```
┌──────────────────────────────────────────────────────────────────┐
│  ⌨ KeyStats                              Today ▾  ● Live   ⋯   │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  TODAY'S STATS              │  KEYBOARD HEATMAP                  │
│  ─────────────              │  ────────────────                  │
│  Total: 12,847              │  ┌───┬───┬───┬───┬───┐            │
│                             │  │ Q │ W │ E │ R │ T │ ...        │
│  TOP KEYS                   │  ├───┼───┼───┼───┼───┤            │
│  1. Space     ████  2,103   │  │ A │ S │ D │ F │ G │ ...        │
│  2. E         ███   1,024   │  └───┴───┴───┴───┴───┘            │
│  ...                        │                                    │
└──────────────────────────────────────────────────────────────────┘
```

点击右上角 `[⋯]` 后展开下拉菜单：

```
┌──────────────────────────────────────────────────────────────────┐
│  ⌨ KeyStats                              Today ▾  ● Live   ⋯   │
│                                                        ┌─────────┴──┐
│                                                        │  刷新记录   │
│                                                        │ ────────── │
│                                                        │  状态信息   │
│                                                        │  设置     │
│                                                        │ ────────── │
│                                                        │  最小化    │
│                                                        │  退出应用   │
│                                                        └────────────┘
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  TODAY'S STATS              │  KEYBOARD HEATMAP                  │
│  ...                        │  ...                               │
└──────────────────────────────────────────────────────────────────┘
```

**同时支持：顶部栏任意位置右键菜单**

```
在标题栏空白处或 Logo 区域右键：
┌──────────────────────┐
│  刷新记录             │
│ ─────────────────    │
│  状态信息             │
│  设置                │
│ ─────────────────    │
│  最小化              │
│  退出应用             │
└──────────────────────┘
```

### 2.3 菜单项功能定义

| 菜单项 | 前端行为 | 后端/Runtime 调用 |
|--------|----------|------------------|
| **刷新记录** | 立即调用 `fetchLiveStats()`，跳过轮询等待 | `window.go.main.App.GetTodayStats()` |
| **状态信息** | 弹出小卡片，显示当前运行状态 | Go 端新增 `GetStatus()` 返回钩子状态、今日总数、数据库大小 |
| **设置** | 预留入口，暂时仅显示占位文案或空弹窗 | — |
| **最小化** | 最小化到任务栏 | `runtime.WindowMinimise()` |
| **退出应用** | 先优雅关闭（flush DB、停钩子），再退出进程 | `app.Shutdown()` + `runtime.Quit()` |

### 2.4 前端实现要点

在 `App.svelte` 右上角添加 `[⋯]` 按钮：

- 样式：`text-text-tertiary hover:text-text-primary`，极简无背景
- 点击展开绝对定位下拉菜单：`absolute right-0 top-full`
- 菜单背景：`bg-surface-raised border border-surface-overlay rounded-lg shadow-lg`
- 菜单项：`px-4 py-2 hover:bg-surface-overlay cursor-pointer transition-colors`
- 点击外部自动关闭菜单（监听 `document` 的 `click` 事件）

右键菜单：
- 在顶部栏 `<div>` 上监听 `on:contextmenu`
- `preventDefault()` 阻止浏览器默认右键菜单
- 显示与 `[⋯]` 菜单内容相同的自定义菜单
- 同样支持点击外部关闭

### 2.5 后端新增 API（建议）

```go
// pkg/app/app.go（或保持现有位置）

// GetStatus 返回应用运行状态信息
func (a *App) GetStatus() (map[string]interface{}, error) {
    return map[string]interface{}{
        "logging":   a.keyboard != nil && a.keyboard.IsActive(),
        "totalKeys": stats.GetTodayTotal(a.database.GetConn()),
        "dbPath":    a.database.GetPath(),
    }, nil
}
```

---

## 三、实施建议

1. **先备份当前代码**：`git branch backup-before-restructure`
2. **先迁移结构，再添加菜单**：结构变动会影响 import 路径，先搞定再增量开发
3. **测试最小化/退出**：确保 `Shutdown()` 能正确 flush 数据库、停止钩子
4. **右键菜单注意 z-index**：确保菜单浮在所有卡片之上

---

## 四、不在这版实现的内容（留作后续）

- 托盘图标（System Tray）最小化
- 开机自启动
- 数据保留天数设置
- 主题切换
