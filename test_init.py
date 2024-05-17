# demo
import subprocess

# 循环从1到1000
for i in range(1, 1001):
    # 构建命令字符串
    command = f"./client set k{i} {i}"
    
    # 执行命令
    try:
        result = subprocess.run(command, shell=True, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        # 打印命令的标准输出和标准错误
        print(f"执行命令: {command}")
        print("标准输出:", result.stdout.decode().strip())
        
    except subprocess.CalledProcessError as e:
        # 如果命令执行失败，打印错误信息
        print(f"命令 '{command}' 执行失败。")
        print("错误输出:", e.stderr.decode().strip())
    
    # 根据需要添加等待时间
    # time.sleep(1) # 例如，每次执行后等待1秒