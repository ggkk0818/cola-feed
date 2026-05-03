在当前目录创建单片机配网静态页vue项目，具体要求：
1.路由默认进入配网页面，页面由网络名称输入框、扫描网络按钮、密码输入框、连接按钮组成；
2.进入页面后输入框默认置空，当用户点击网络名称输入框时弹出网络选择对话框，第一次打开对话框自动扫描网络；
3.用户在网络选择对话框点击网络后，对话框关闭，将选择的网络名称填入输入框；
4.用户在网络选择对话框可以点击“输入网络名称”按钮手动键入网络名；
5.用户在网络选择对话框可以点击“扫描网络”手动刷新网络列表，注意设置loading状态防止重复点击；
6.用户填写网络名与密码后点击连接按钮，先请求获取rsa加密公钥接口，使用公钥加密密码，再请求网络连接接口，页面提示“网络连接中，请查看设备屏幕获取连接状态。”
7.获取网络列表接口：GET /api/wifi/list,返回值
{
    "code": 200,
    "data": [
        {"ssid": "network1"},
        {"ssid": "network2"},
        {"ssid": "network3"}
    ]
}
8.获取rsa加密公钥接口：GET /api/rsa/public_key,返回值
{
    "code": 200,
    "data": {
        "public_key": "-----BEGIN PUBLIC KEYMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQ..."
        }
}
9.连接网络接口：POST /api/wifi/connect,请求体
{
    "ssid": "network1",
    "password": "encrypted_password"
},返回值
{
    "code": 200,
    "message": "连接请求已发送"
}
10.请使用Vue 3和Vue Router创建项目，并使用Axios进行API请求；
11.wifi图标文件C:\Users\ggkk2\Downloads\WIFI.svg
12.密码图标文件C:\Users\ggkk2\Downloads\sq-password.svg
13.扫描网络图标文件C:\Users\ggkk2\Downloads\scan-search.svg
14.页面样式请简洁美观，使用CSS或SCSS进行样式设计，支持响应式布局，支持暗色模式；
15.网络选择对话框交互需适配移动端和桌面端，使用Vue的组件系统进行开发；
16.请确保代码结构清晰，组件划分合理，注释充分，便于后续维护和扩展；
17.打包尽量减少文件数量；
