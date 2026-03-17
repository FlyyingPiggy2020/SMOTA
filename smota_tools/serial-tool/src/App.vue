<script setup>
import HeaderMenu from './components/HeaderMenu.vue'
import SidebarIcon from './components/SidebarIcon.vue'
import SidebarConfig from './components/SidebarConfig.vue'
import MainContent from './components/MainContent.vue'
import { themeOverrides } from './plugins/naive-ui'
import { ref } from 'vue'

const collapsed = ref(false)
const activePanel = ref('serial') // 当前选中的配置面板

function handleSelectPanel(panel) {
    if (activePanel.value === panel) {
        // 点击同一个图标，切换展开/收起
        collapsed.value = !collapsed.value
    } else {
        // 点击不同图标，切换面板并展开
        activePanel.value = panel
        collapsed.value = false
    }
}
</script>

<template>
    <n-config-provider :theme-overrides="themeOverrides" class="h-full">
        <n-layout position="absolute">
            <div class="h-full flex flex-col">
                
                <!-- Header -->
                <n-layout-header class="h-12 flex-none border-b-2 border-gray-600 box-border z-20 relative">
                    <HeaderMenu />
                </n-layout-header>

                <!-- 下方主体 -->
                <div class="flex-1 flex overflow-hidden relative">
                    <!-- 左侧图标栏 -->
                    <div class="w-12 h-full flex-none bg-gray-50 border-r-2 border-gray-600 z-10">
                        <SidebarIcon
                            :active="activePanel"
                            @update:active="handleSelectPanel"
                        />
                    </div>

                    <!-- 中间配置面板 -->
                    <div
                        class="h-full flex-none transition-all duration-300 ease-in-out border-r-2 border-gray-600 overflow-hidden bg-white"
                        :style="{ width: collapsed ? '0px' : '250px', borderWidth: collapsed ? '0px' : '' }"
                    >
                        <div 
                            class="w-[250px] h-full transition-opacity duration-200 whitespace-nowrap"
                            :class="collapsed ? 'opacity-0 delay-0' : 'opacity-100 delay-100'"
                        >
                            <SidebarConfig :panel="activePanel" />
                        </div>
                    </div>

                    <!-- 右侧主内容 -->
                    <div class="flex-1 h-full w-0 overflow-auto bg-gray-50/50 p-4">
                        <MainContent />
                    </div>
                </div>
            </div>
            
        </n-layout>
    </n-config-provider>
</template>


<style scoped>

</style>
