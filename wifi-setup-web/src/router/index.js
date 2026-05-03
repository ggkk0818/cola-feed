import { createRouter, createWebHashHistory } from 'vue-router'
import WifiSetupView from '../views/WifiSetupView.vue'

const routes = [
  {
    path: '/',
    name: 'wifi-setup',
    component: WifiSetupView,
  },
  {
    path: '/:pathMatch(.*)*',
    redirect: '/',
  },
]

const router = createRouter({
  history: createWebHashHistory(),
  routes,
})

export default router