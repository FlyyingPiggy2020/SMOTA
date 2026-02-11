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
    <n-config-provider :theme-overrides="themeOverrides">
        <n-layout class="flex-1">
            <n-layout-header class="border-b-2 border-gray-600 h-12">
                <HeaderMenu />
            </n-layout-header>

            <div class="flex h-[calc(100%-48px)]">
                <!-- 最左侧：常驻图标栏 -->
                <div >
                    <SidebarIcon
                        :active="activePanel"
                        @update:active="handleSelectPanel"
                    />
                </div>

                <!-- 中间：配置面板 -->
                <div
                    class="border-r-2 border-gray-600 transition-all duration-300 overflow-hidden"
                    :style="{ width: collapsed ? '0px' : '250px' }"
                >
                    <div
                        v-show="!collapsed"
                        class="w-64 h-full transition-opacity duration-300 "
                        :class="collapsed ? 'opacity-0' : 'opacity-100'"
                    >
                        <SidebarConfig :panel="activePanel" />
                    </div>
                </div>

                <!-- 右侧：主内容 -->
                <div class="flex-1 h-screen">
                    <MainContent />
                </div>
            </div>
        </n-layout>
    </n-config-provider>
</template>


<style scoped>

</style>
