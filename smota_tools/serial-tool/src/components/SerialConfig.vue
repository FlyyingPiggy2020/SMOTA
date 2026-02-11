<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { invoke } from '@tauri-apps/api/core'

// 端口选项（动态加载）
const portOptions = ref<Array<{ label: string; value: string }>>([])
const loading = ref(false)
const baudRateValue = ref<number>(115200)

// 刷新端口列表
const refreshPorts = async () => {
    loading.value = true
    try {
        const ports = await invoke<Array<{ name: string; port_type: string }>>('list_ports')
        portOptions.value = ports.map(p => ({
            label: `${p.name} (${p.port_type})`,
            value: p.name
        }))
    } catch (error) {
        console.error('获取串口列表失败:', error)
    } finally {
        loading.value = false
    }
}

onMounted(() => {
    refreshPorts()
})

// 波特率选项（常用值）
const baudRateOptions = [
    { label: '300', value: 300 },
    { label: '600', value: 600 },
    { label: '1200', value: 1200 },
    { label: '2400', value: 2400 },
    { label: '4800', value: 4800 },
    { label: '9600', value: 9600 },
    { label: '14400', value: 14400 },
    { label: '19200', value: 19200 },
    { label: '28800', value: 28800 },
    { label: '38400', value: 38400 },
    { label: '57600', value: 57600 },
    { label: '115200', value: 115200 },
    { label: '230400', value: 230400 },
    { label: '460800', value: 460800 },
    { label: '921600', value: 921600 },
];

// 自定义输入的波特率
const customBaudRate = ref<number>(0)
// 是否显示自定义输入框
const showCustomInput = ref(false)
// 输入框引用
const inputRef = ref<HTMLInputElement | null>(null)

// 双击切换自定义输入模式
const onBaudRateDoubleClick = () => {
    customBaudRate.value = baudRateValue.value
    showCustomInput.value = true
    // 下次渲染后聚焦输入框
    setTimeout(() => {
        inputRef.value?.focus()
    }, 0)
}

// 完成自定义输入
const onCustomBaudRateConfirm = () => {
    const num = customBaudRate.value
    if (!isNaN(num) && num > 0) {
        baudRateValue.value = num
    }
    showCustomInput.value = false
}

// 键盘事件处理
const onCustomBaudRateKeydown = (e: KeyboardEvent) => {
    if (e.key === 'Enter') {
        onCustomBaudRateConfirm()
    } else if (e.key === 'Escape') {
        showCustomInput.value = false
    }
}

const dataBitsOptions = [
    { label: '5', value: 5 },
    { label: '6', value: 6 },
    { label: '7', value: 7 },
    { label: '8', value: 8 },
];

const stopBitsOptions = [
    { label: '1', value: 1 },
    { label: '2', value: 2 },
];

const parityOptions = [
    { label: '无', value: 'none' },
    { label: '奇校验', value: 'odd' },
    { label: '偶校验', value: 'even' },
];

</script>

<template>
    <NScrollbar style="max-height: calc(100vh - 100px);">
        <div class="p-4">
            <div class="flex justify-between items-center mb-4">
                <!-- 状态标签 -->
                <div class="text-sm">
                    <span class="text-green-500">●</span> 已连接
                </div>
                <!-- 刷新端口按钮 -->
                <NButton size="small" :loading="loading" @click="refreshPorts">刷新端口</NButton>
            </div>
            <!-- 打开/断开按钮 -->
            <NButton type="primary" block class="mb-4!">
                打开串口
            </NButton>

            <!-- 卡片1: 串口参数配置 (可折叠) -->
            <NCard size="small" class="mb-4">
                <NCollapse>
                    <NCollapseItem title="串口参数配置" name="serial-config">
                        <NGrid :cols="1" :x-gap="4" :y-gap="6">
                            <!-- 端口号 -->
                            <NGi>
                                <label class="block text-sm mb-1">端口号</label>
                                <NSelect placeholder="选择端口" :options="portOptions" />
                            </NGi>

                            <!-- 波特率 -->
                            <NGi>
                                <label class="block text-sm mb-1">波特率</label>
                                <NInputNumber
                                    v-if="showCustomInput"
                                    ref="inputRef"
                                    v-model:value="customBaudRate"
                                    class="w-full"
                                    :min="0"
                                    :max="4294967295"
                                    placeholder="输入波特率"
                                    @blur="onCustomBaudRateConfirm"
                                    @keydown="onCustomBaudRateKeydown"
                                />
                                <NSelect
                                    v-else
                                    :options="baudRateOptions"
                                    :value="baudRateValue"
                                    @update:value="(val: number) => baudRateValue = val"
                                    @dblclick="onBaudRateDoubleClick"
                                />
                            </NGi>

                            <!-- 数据位 -->
                            <NGi>
                                <label class="block text-sm mb-1">数据位</label>
                                <NSelect :options="dataBitsOptions" />
                            </NGi>

                            <!-- 停止位 -->
                            <NGi>
                                <label class="block text-sm mb-1">停止位</label>
                                <NSelect :options="stopBitsOptions" />
                            </NGi>

                            <!-- 校验位 -->
                            <NGi>
                                <label class="block text-sm mb-1">校验位</label>
                                <NSelect :options="parityOptions" />
                            </NGi>
                        </NGrid>
                    </NCollapseItem>
                </NCollapse>
            </NCard>

            <!-- 卡片2: 流控信号控制 (可折叠) -->
            <NCard size="small">
                <NCollapse>
                    <NCollapseItem title="流控信号控制" name="flow-control">
                        <NGrid :cols="3" :x-gap="16">
                            <!-- DTR -->
                            <NGi>
                                <div class="text-center">
                                    <div class="font-bold">DTR</div>
                                    <div class="text-gray-500 text-xs">数据就绪</div>
                                    <NSwitch class="mt-2" />
                                </div>
                            </NGi>

                            <!-- RTS -->
                            <NGi>
                                <div class="text-center">
                                    <div class="font-bold">RTS</div>
                                    <div class="text-gray-500 text-xs">请求发送</div>
                                    <NSwitch class="mt-2" />
                                </div>
                            </NGi>

                            <!-- BREAK -->
                            <NGi>
                                <div class="text-center">
                                    <div class="font-bold">BREAK</div>
                                    <div class="text-gray-500 text-xs">中断信号</div>
                                    <NSwitch class="mt-2" />
                                </div>
                            </NGi>
                        </NGrid>
                    </NCollapseItem>
                </NCollapse>
            </NCard>
        </div>
    </NScrollbar>
</template>
