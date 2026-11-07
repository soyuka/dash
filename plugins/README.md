# Plugins

Every `plugins/<type>/<name>/` directory is built into the Qt plugin
`lib/dash/plugins/<type>/lib<name>.so` from the `.cpp` files it contains. Dash looks
for plugins in `<dir of the dash binary>/../lib/dash/plugins/<type>`.

Types: `brightness`, `radio`, `vehicle`, `launcher`.
