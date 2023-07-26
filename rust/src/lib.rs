/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

use std::sync::Arc;
use vulkano::device::physical::PhysicalDevice;
use vulkano::device::physical::PhysicalDeviceType;
use vulkano::device::DeviceCreateInfo;
use vulkano::device::DeviceExtensions;
use vulkano::device::Queue;
use vulkano::device::QueueCreateInfo;
use vulkano::device::QueueFlags;
use vulkano::{
    device::Device,
    instance::{Instance, InstanceCreateInfo},
    swapchain::Surface,
    VulkanLibrary,
};
use vulkano::{Handle, VulkanObject};
use vulkano_win::VkSurfaceBuild;
use winit::{
    event_loop::EventLoop,
    window::{Fullscreen, WindowBuilder},
};

use winit::event::{Event, WindowEvent};
use winit::event_loop::ControlFlow;

pub struct MyContext {
    pub library: Arc<VulkanLibrary>,
    pub instance: Arc<Instance>,
    pub surface: Arc<Surface>,
    pub event_loop: EventLoop<()>,
    pub device: Arc<Device>,
    pub physical_device: Arc<PhysicalDevice>,
    pub queue: Arc<Queue>,
}

#[cxx::bridge(namespace = "sp::rs")]
mod ffi_rust {
    extern "Rust" {
        type MyContext;
        fn create_context() -> Box<MyContext>;
        fn get_surface_handle(context: &MyContext) -> u64;
        fn get_physical_device_handle(context: &MyContext) -> u64;
        fn get_instance_handle(context: &MyContext) -> u64;
        fn run_event_loop(context: &mut MyContext);
        fn print_hello();
    }
}

mod wasmer_vm;

fn print_hello() {
    println!("hello world!");

    let result = wasmer_vm::run_wasm();
    if result.is_err() {
        println!("run_wasm() failed! {}", result.err().unwrap());
    }
}

pub fn select_physical_device(
    instance: &Arc<Instance>,
    surface: &Arc<Surface>,
    device_extensions: &DeviceExtensions,
) -> (Arc<PhysicalDevice>, u32) {
    instance
        .enumerate_physical_devices()
        .expect("failed to enumerate physical devices")
        .filter(|p| p.supported_extensions().contains(device_extensions))
        .filter_map(|p| {
            p.queue_family_properties()
                .iter()
                .enumerate()
                .position(|(i, q)| {
                    q.queue_flags.contains(QueueFlags::GRAPHICS)
                        && p.surface_support(i as u32, surface).unwrap_or(false)
                })
                .map(|q| (p, q as u32))
        })
        .min_by_key(|(p, _)| match p.properties().device_type {
            PhysicalDeviceType::DiscreteGpu => 0,
            PhysicalDeviceType::IntegratedGpu => 1,
            PhysicalDeviceType::VirtualGpu => 2,
            PhysicalDeviceType::Cpu => 3,
            _ => 4,
        })
        .expect("no device available")
}

unsafe impl Send for MyContext {}
unsafe impl Sync for MyContext {}

fn create_context() -> Box<MyContext> {
    let library: Arc<VulkanLibrary> = VulkanLibrary::new().expect("no local Vulkan library/DLL");
    let mut required_extensions: vulkano::instance::InstanceExtensions =
        vulkano_win::required_extensions(&library);
    /*
    extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
        extensions.emplace_back("VK_KHR_wayland_surface"); // FIXME: Platform specific.
        extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    */
    required_extensions.khr_surface = true;
    required_extensions.khr_wayland_surface = true;
    required_extensions.khr_get_physical_device_properties2 = true;
    required_extensions.ext_debug_utils = true;

    let instance: Arc<Instance> = Instance::new(
        library.clone(),
        InstanceCreateInfo {
            enabled_extensions: required_extensions,
            enabled_layers: vec!["VK_LAYER_KHRONOS_validation".to_string()],
            ..Default::default()
        },
    )
    .expect("failed to create instance");

    let event_loop = EventLoop::new(); // ignore this for now
    let monitor = event_loop
        .available_monitors()
        .next()
        .expect("no monitor found!");
    let fullscreen = Some(Fullscreen::Borderless(Some(monitor.clone())));
    let surface = WindowBuilder::new()
        .with_title("STRAY PHOTONS")
        .with_fullscreen(fullscreen)
        .build_vk_surface(&event_loop, instance.clone())
        .unwrap();

    let device_extensions = DeviceExtensions {
        khr_swapchain: true,
        ..DeviceExtensions::empty()
    };

    let (physical_device, queue_family_index) =
        select_physical_device(&instance, &surface, &device_extensions);

    let (device, mut queues) = Device::new(
        physical_device.clone(),
        DeviceCreateInfo {
            queue_create_infos: vec![QueueCreateInfo {
                queue_family_index,
                ..Default::default()
            }],
            enabled_extensions: device_extensions, // new
            ..Default::default()
        },
    )
    .expect("failed to create device");

    let queue = queues.next().unwrap();

    println!("Hello, Rust!");
    Box::new(MyContext {
        library: library.to_owned(),
        instance: instance.to_owned(),
        surface: surface.to_owned(),
        event_loop,
        device: device.to_owned(),
        physical_device: physical_device.to_owned(),
        queue: queue.to_owned(),
    })
}

fn get_surface_handle(context: &MyContext) -> u64 {
    println!("Library API version: {}", context.library.api_version());
    println!("Instance API version: {}", context.instance.api_version());
    context.surface.handle().as_raw()
}

fn get_physical_device_handle(context: &MyContext) -> u64 {
    context.device.physical_device().handle().as_raw()
}

fn get_instance_handle(context: &MyContext) -> u64 {
    context.instance.handle().as_raw()
}

fn run_event_loop(context: &mut MyContext) {
    let event_loop = std::mem::take(&mut context.event_loop);
    event_loop.run(move |event, _, control_flow| match event {
        Event::WindowEvent {
            event: WindowEvent::CloseRequested,
            ..
        } => {
            *control_flow = ControlFlow::Exit;
        }
        _ => (),
    });
}
