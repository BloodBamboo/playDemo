package cn.com.bamboo.easy_common.help

data class PermissionsInfo(
    var name: String,
    var shouldShowRequestPermissionRationale: Boolean,
    var granted: Boolean
)