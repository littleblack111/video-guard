# video-guard
Helper that guards and requests user confirmation for /dev/video* access

# Important
This requires inotifywait and lsof

## LSOF cap_sys_admin+ep PRIVILEGES
To use lsof, you need to have the `cap_sys_admin+ep` capability set on the binary. You can set this capability using the following command:

```bash
sudo setcap cap_sys_admin+ep /usr/bin/lsof
```

# Notes
- No it wouldn't let the app get first few frames before inotifywait kicks in. (i tested it)
