# Toolbox translations

Toolbox UI translations live in one flat JSON object per locale. Translation
keys are stable identifiers used by `toolbox_i18n::tr()`.

When changing UI text, update both `zh-CN.json` and `en-US.json`. Their keys
must match exactly; the build validates this and generates the compile-time C++
catalog automatically. The JSON files are build inputs and do not need to be
copied onto the console at runtime.
