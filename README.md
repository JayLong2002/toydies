# toydies

**一个轻量的kv数据库，参考了redies部分设计和代码**
# 压力测试
* 单次get平均时间0.017ms,单次set平均时间0.0289ms，随机范围查询平均单次时间0.03ms</br>
# 设计思路
## 服务器部分
采用非阻塞IO和轮询来实现对服务器的并发访问
## kv存储部分
* 对于单个的kv存取，采用改进后的渐进式重哈希的哈希表进行存储，O(1) 的时间复杂度 </br>
* 对于范围查询，采用skiplist进行，时间复杂度大概率是在O（log n），小概率O（n）</br>
* 采用skiplist来实现sort-set而不是avl等树状结构，主要是为了后续并发数据结构的升级 </br>
# 支持功能
## set
set k v，set 成功会返回 0
## get
get k ，如果不存在会返回toydies-null，如果存在返回v
## del
del k，如果存在并且删除成功会返回0，其余情况返回toydies-error
## search(范围查询)
search v1 v2 ， 该命令会返回所有v位于v1，v2之间的所有键值对，v2 < v1 是undefine behaviour ，会返回error
# TODO：
## kv部分
对kv存储做分层优化，小于一定规模直接分一块内存，大于一定规模采用skip-list
## server部分
添加线程池对服务器响应作出优化
设计一个合理的调度算法
