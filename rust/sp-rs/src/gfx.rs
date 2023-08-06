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
use winit::dpi::LogicalSize;
use winit::{event_loop::EventLoop, window::WindowBuilder};
use winit::platform::run_return::EventLoopExtRunReturn;

use winit::event::{Event, WindowEvent};
use winit::event_loop::ControlFlow;

pub struct WinitContext {
    pub library: Arc<VulkanLibrary>,
    pub instance: Arc<Instance>,
    pub surface: Arc<Surface>,
    pub event_loop: EventLoop<()>,
    pub device: Arc<Device>,
    pub physical_device: Arc<PhysicalDevice>,
    pub queue: Arc<Queue>,
}

#[cxx::bridge(namespace = "sp::gfx")]
mod ctx {
    extern "Rust" {
        type WinitContext;
        fn create_context(x: i32, y: i32) -> Box<WinitContext>;
        fn get_surface_handle(context: &WinitContext) -> u64;
        fn get_physical_device_handle(context: &WinitContext) -> u64;
        fn get_instance_handle(context: &WinitContext) -> u64;
        fn run_event_loop(context: &mut WinitContext);
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

unsafe impl Send for WinitContext {}
unsafe impl Sync for WinitContext {}

fn create_context(x: i32, y: i32) -> Box<WinitContext> {
    let library: Arc<VulkanLibrary> = VulkanLibrary::new().expect("no local Vulkan library/DLL");
    let mut required_extensions: vulkano::instance::InstanceExtensions =
        vulkano_win::required_extensions(&library);

    required_extensions.khr_surface = true;
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

    let event_loop = EventLoop::new();
    /*let monitor = event_loop
        .available_monitors()
        .next()
        .expect("no monitor found!");
    let fullscreen = Some(Fullscreen::Borderless(Some(monitor.clone())));*/
    let surface = WindowBuilder::new()
        .with_title("STRAY PHOTONS")
        .with_inner_size(LogicalSize {
            width: x,
            height: y,
        })
        //.with_fullscreen(fullscreen)
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
            enabled_extensions: device_extensions,
            ..Default::default()
        },
    )
    .expect("failed to create device");

    let queue = queues.next().unwrap();

    println!("Hello, Rust!");
    Box::new(WinitContext {
        library: library.to_owned(),
        instance: instance.to_owned(),
        surface: surface.to_owned(),
        event_loop,
        device: device.to_owned(),
        physical_device: physical_device.to_owned(),
        queue: queue.to_owned(),
    })
}

fn get_surface_handle(context: &WinitContext) -> u64 {
    println!("Library API version: {}", context.library.api_version());
    println!("Instance API version: {}", context.instance.api_version());
    context.surface.handle().as_raw()
}

fn get_physical_device_handle(context: &WinitContext) -> u64 {
    context.device.physical_device().handle().as_raw()
}

fn get_instance_handle(context: &WinitContext) -> u64 {
    context.instance.handle().as_raw()
}

fn run_event_loop(context: &mut WinitContext) {
    // let event_loop = std::mem::take(&mut context.event_loop);
    context.event_loop.run_return(|event, _, control_flow| match event {
        Event::WindowEvent {
            event: WindowEvent::CloseRequested,
            ..
        } => {
            *control_flow = ControlFlow::Exit;
        },
        _ => {
            *control_flow = ControlFlow::Exit;
        },
    });
}
