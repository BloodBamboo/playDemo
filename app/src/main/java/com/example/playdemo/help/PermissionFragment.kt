package cn.com.bamboo.easy_common.help

import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment

/**
 * 权限申请辅助fragment
 */
class PermissionFragment : Fragment() {
    private val mRequest = 111//生产请求code

    var shouldShowRequest = false

    fun permission(permissions: Array<String>) {
        if (check(permissions)) {
            Permission4MultipleHelp.permissionSuccess()
        } else {
           requestPermissions(permissions, mRequest)
        }
    }

    /**
     * 检查权限是否已经拥有
     */
    private fun check(permissions: Array<String>): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return true
        } else {
            var result = true
            for (p in permissions) {
                if (ContextCompat.checkSelfPermission(context!!, p)
                    != PackageManager.PERMISSION_GRANTED
                ) {
                    return false
                }
            }
            return result
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        if (requestCode == mRequest) {
            if (shouldShowRequest) {
                val list = mutableListOf<PermissionsInfo>()
                for (i in permissions.indices) {
                    list.add(
                        PermissionsInfo(
                            permissions[i],
                            shouldShowRequestPermissionRationale(permissions[i]),
                            grantResults[i] == PackageManager.PERMISSION_GRANTED
                        )
                    )
                }
                Permission4MultipleHelp.onPermissionShowRequest(list)
            } else {
                Permission4MultipleHelp.onPermission(permissions, grantResults)
            }
        }
    }
}