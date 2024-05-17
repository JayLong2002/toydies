# 定义目标可执行文件名
SERVER = server
CLIENT = client

# 定义服务器程序的源文件
SERVER_SOURCES = server.cc Node.h skiplist.h kv.h command.h connection.h hashtable.cc hashtable.h
# 对源文件进行排序并去除重复，然后转换为.o文件
SERVER_OBJECTS = $(SERVER_SOURCES:.cc=.o)

# 定义客户端程序的源文件
CLIENT_SOURCES = client.cc
# 客户端没有头文件依赖，如果有，也需要按上面的方式处理
CLIENT_OBJECTS = $(CLIENT_SOURCES:.cpp=.o)

# 定义编译器和链接器
CXX = g++

# 默认目标是编译服务器和客户端程序
all: $(SERVER) $(CLIENT)

# 链接服务器程序
$(SERVER): $(SERVER_OBJECTS)
	$(CXX) -o $@ $^

# 链接客户端程序
$(CLIENT): $(CLIENT_OBJECTS)
	$(CXX) -o $@ $^

# 清理生成的.o文件和可执行文件
clean:
	rm client hashtable.o server server.o

.PHONY: all clean