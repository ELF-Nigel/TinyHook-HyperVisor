# hv control interface (non-operational hv actions)

## scope
this control plane exposes a versioned, bounded io interface for usermode tooling. it does not perform hypervisor launch, vm-exit handling, or runtime feature enforcement. configuration updates are staged only.

## device
- nt device: `\\device\\hvcontrol`
- dos device: `\\dosdevices\\hvcontrol`
- usermode path: `\\.\\hvcontrol`

## ioctls
- `ioctl_hv_get_status`
- `ioctl_hv_get_config`
- `ioctl_hv_set_config`
- `ioctl_hv_enable_feature`
- `ioctl_hv_disable_feature`
- `ioctl_hv_query_cpu`
- `ioctl_hv_get_log`
- `ioctl_hv_list_features`
- `ioctl_hv_get_exit_stats`
- `ioctl_hv_get_hvlog`

see `include/public/hv_control.h` for full definitions.

## message format
all requests and responses are fixed-size and versioned.

- `hvctl_request_t`
  - header + payload (config, feature, control log cursor, or hv log cursor)
- `hvctl_response_t`
  - header + result + payload (status, config, log chunk, feature list, exit stats, or hv log chunk)

## state model
- `driver_state`: stopped / starting / active / error
- `hv_state`: off / initialized / running

status contains `policy_flags` to indicate staged-only config application.

## safety rules
- no raw pointers cross usermode/kernel
- fixed-size buffers only (method buffered)
- unknown commands return `hvctl_res_not_supported`
- invalid input sizes return `status_buffer_too_small`
- config validation rejects out-of-range values

## logging
two ring buffers exist:
- control-plane log: `ioctl_hv_get_log`
- hv instrumentation log: `ioctl_hv_get_hvlog`

both return chunks after a cursor.

## extensions
future additions should:
- bump `hv_control_version`
- preserve old struct sizes or add `size` fields per payload
- keep writes staged until a verified hv backend applies them
