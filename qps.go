package main

import (
	"fmt"
	"os/exec"
	"sync"
	"time"
)

// runCommand 执行给定的命令，并在完成后通知WaitGroup。
func runCommand(cmdName string, args []string, wg *sync.WaitGroup, threadID int) {
	defer wg.Done()

	// 构造命令
	cmd := exec.Command(cmdName, args...)

	// 记录开始执行的时间
	start := time.Now()

	// 执行命令并捕获输出
	out, err := cmd.CombinedOutput()
	if err != nil {
		// 如果有错误发生，打印错误信息
		fmt.Printf("Thread %d: An error occurred. Error: %v\n", threadID, err)
	} else {
		// 如果命令执行成功，打印输出
		fmt.Printf("Thread %d: Command executed successfully. Output: %s\n", threadID, out)
	}

	// 打印当前goroutine的执行时间
	fmt.Printf("Thread %d: Execution time: %s\n", threadID, time.Since(start))
}

func main() {
	// 定义WaitGroup用于等待所有goroutine完成
	var wg sync.WaitGroup

	// 设置要启动的goroutine数量
	totalThreads := 10000

	// 记录所有goroutine启动的时间
	start := time.Now()

	// 启动多个goroutine执行命令
	for i := 0; i < totalThreads; i++ {
		wg.Add(1) // 增加WaitGroup的计数
		//go runCommand("./client1", []string{"set", fmt.Sprintf("k%d", i), "1"}, &wg, i)
		go runCommand("./test_qps", []string{"get", "k2"}, &wg, i)
	}

	// 等待所有goroutine完成
	wg.Wait()
	elapsed := time.Since(start).Seconds() // 总时间（秒）
	qps := float64(totalThreads) / elapsed // 计算QPS

	// 打印所有goroutine完成的总时间
	fmt.Printf("All threads completed. Total time: %s\n", time.Since(start))
	fmt.Printf("QPS: %.0f\n", qps)
}
