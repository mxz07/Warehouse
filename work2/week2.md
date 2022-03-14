# *java性能测试——插入排序*
## 1.过程
   - 在idealc中下载<font color=Blue>Jprofile</font>插件
   - 下载<font color=Blue>Jprofile</font>,Setting——tools——jprofile设置包位置
   - <font color=Blue>Jprofile</font>中进行会话设置
## 2.结论
   - 性能测试结果如图所示：
   -  <img src="1.png" width="100%">
   - 根据其封装的上下文做出系统资源访问决策即<font color=Red>AccessControlContext</font>
  的操作耗时最长。