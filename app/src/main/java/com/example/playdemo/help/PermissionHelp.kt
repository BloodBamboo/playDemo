package cn.com.bamboo.easy_common.help

import android.content.pm.PackageManager
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import org.jetbrains.anko.toast
import java.lang.ref.WeakReference

object Permission4MultipleHelp {
    val TGA = "PermissionFragment"
    //申请成功回调函数
    private var mSuccess: (() -> Unit)? = null
    //申请没有全部成功回调函数
    private var mFail: ((List<String>?) -> Unit)? = null
    //申请没有全部成功，并且用户勾选不再提示选项回调
    private var mShowRequestFail: ((List<PermissionsInfo>?) -> Unit)? = null

    /**
     * 申请对应activity所需权限列表
     */
    fun request(
        activity: FragmentActivity, permissions: Array<String>,
        success: (() -> Unit)? = null,
        fail: ((List<String>?) -> Unit)? = null,
        showRequestFail: ((List<PermissionsInfo>?) -> Unit)? = null,
        shouldShowRequest: Boolean = false
    ) {
        initCallback(activity, success, fail, showRequestFail)
        val fragment = getFragment(activity, shouldShowRequest)
        fragment.permission(permissions)
    }

    /**
     * 回调方法初始化
     */
    private fun initCallback(
        activity: FragmentActivity,
        success: (() -> Unit)? = null,
        fail: ((List<String>?) -> Unit)? = null,
        showRequestFail: ((List<PermissionsInfo>?) -> Unit)? = null
    ): Permission4MultipleHelp {
        val temp = WeakReference<FragmentActivity>(activity)
        mSuccess = if (success == null) {
            {
                temp.get()?.run {
                    toast("权限通过")
                }
            }
        } else success


        mFail = if (fail == null) {
            {
                temp.get()?.run {
                    toast("请允许同意所需权限")
                }
            }
        } else fail

        mShowRequestFail = if (showRequestFail == null) {
            {
                temp.get()?.run {
                    toast("请去设置 - 应用程序，开启所需权限")
                }
            }

        } else showRequestFail
        return this
    }

    /**
     * 查询权限申请所需fragment
     * @param shouldShowRequest 是否返回带shouldShowRequest值的类型
     */
    private fun getFragment(
        activity: FragmentActivity,
        shouldShowRequest: Boolean
    ): PermissionFragment {
        var temp: Fragment?

        val fragmentManger = activity.supportFragmentManager
        temp = fragmentManger.findFragmentByTag(TGA)

        if (temp == null) {
            temp = PermissionFragment()
            fragmentManger.beginTransaction().add(temp, TGA)
                .commitNow()
        }

        (temp as PermissionFragment).shouldShowRequest = shouldShowRequest
        return temp
    }

    /**
     * 权限验证已拥有
     */
    fun permissionSuccess() {
        mSuccess?.run {
            this()
        }
    }

    /**
     * 申请结果处理，返回申请成功还是失败
     */
    fun onPermission(
        permissions: Array<String>,
        grantResults: IntArray
    ) {

        if (grantResults.isEmpty()) {
            mFail?.run {
                this(null)
            }
            return
        }
        var result = true
        var permissionList = mutableListOf<String>()
        for (i in grantResults.indices) {
            if (grantResults[i] != PackageManager.PERMISSION_GRANTED) {
                result = false
                permissionList.add(permissions[i])
            }
        }
        if (result) {
            mSuccess?.run {
                this()
            }
        } else {
            mFail?.run {
                this(permissionList)
            }
        }
    }


    /**
     * 申请结果处理，返回失败权限信息列表
     */
    fun onPermissionShowRequest(list: MutableList<PermissionsInfo>) {
        if (list.isEmpty()) {
            mFail?.run {
                this(null)
            }
            return
        }
        var result = true
        var showRequest = false
        val permissions = mutableListOf<PermissionsInfo>()
        var permissionList = mutableListOf<String>()
        for (i in list.indices) {
            if (!list[i].granted) {
                result = false
                if (!list[i].shouldShowRequestPermissionRationale) {
                    showRequest = true
                    permissions.add(list[i])
                }
            }
        }
        when {
            result -> mSuccess?.run {
                this()
            }
            showRequest -> {
                mShowRequestFail?.run {
                    this(permissions)
                }
            }
            else -> mFail?.run {
                this(permissionList)
            }
        }
    }

}