from setuptools import find_packages, setup

package_name = 'vlm_pkg'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['rclpy','std_msg','pydub','requests'],
    zip_safe=True,
    maintainer='nvidia',
    maintainer_email='dorise0126@gmail.com',
    description='TODO: vlm package',
    license='Apache-2.0',
    entry_points={
        'console_scripts': [
            'trigger_node = vlm_pkg.TriggerNode:main',
            'tts_node = vlm_pkg.TTSnode:main',
        ],
    },
)
