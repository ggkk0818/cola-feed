# Wi-Fi Setup Web

单片机配网静态页面，基于 Vue 3 + Vue Router + Axios。

## 开发命令

- 安装依赖: npm install
- 本地开发: npm run dev
- 生产构建: npm run build
- 本地预览: npm run preview

## 主要功能

- 默认路由直达配网页面
- SSID 输入框点击后弹出网络选择对话框
- 对话框首次打开自动扫描网络
- 支持手动输入网络名称
- 扫描网络按钮具备 loading 防重复点击
- 连接时先获取 RSA 公钥，加密密码后提交连接接口

## 接口约定

- 获取网络列表: GET /api/wifi/list
- 获取公钥: GET /api/rsa/public_key
- 提交连接: POST /api/wifi/connect

## 打包优化

- 通过 Vite 配置禁用 CSS 分包并调整输出命名
- 默认产物尽量收敛为少量核心文件
