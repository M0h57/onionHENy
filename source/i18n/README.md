# Shared translations

User-facing translations live in one JSON file per locale. Each file has
two sections:

- `toolbox`: stable keys consumed by `toolbox_i18n::tr()`.
- `notifications`: exact English notification source strings mapped to the
  localized text consumed by `onion_notify_tr()`.

Update both `zh-CN.json` and `en-US.json` when adding or changing text. The
build rejects missing keys, invalid values, changed English notification
source keys, and mismatched `printf` conversion specifications. It then
generates C/C++ tables in the build directory and embeds them in the ELF,
so no locale files are required on the console at runtime.
