boot status telemetry (efi → windows → discord)

overview
- efi writes `elf_boot_status` as a uefi runtime variable
- windows startup app reads it and posts a discord webhook
- after send, the variable is deleted to avoid duplicates

efi variable
name: `elf_boot_status`
guid: `{4c7aaf1d-8f1f-4a0b-9e34-528e214612a9}`

windows startup app
binary: `src/tools/bootstatus_service.c` (console app)
requires: privilege `se_system_environment_name`
configuration file: `c:\programdata\elfhv\bootstatus_webhook.txt`

install (simple startup task)
1) compile the tool and place it in `c:\program files\elfhv\bootstatus_service.exe`
2) create webhook file at `c:\programdata\elfhv\bootstatus_webhook.txt`
3) create a startup task:

```
schtasks /create /tn elfhv_boot_status /sc onstart /ru system /tr "c:\program files\elfhv\bootstatus_service.exe --once"
```

notes
- no memory addresses or internal data are included in the payload
- the efi variable is deleted after a successful send
- payload fields: hv enabled, hv initialized, virt supported, ept supported, config loaded, hv version, boot timestamp
