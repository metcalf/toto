target = node[:openwrt][:target]
buildpath = node[:openwrt][:buildpath] = node[:openwrt][:buildpath].chomp("/")
if not node[:openwrt][:source].start_with?("/")
  node[:openwrt][:source] = "/" + node[:openwrt][:source]
end
source = node[:openwrt][:source] = node[:openwrt][:source].chomp("/")
source_name = source.split("/")[-1]
buildroot = node[:openwrt][:buildroot] = buildpath + "/" + target + "-" + source_name

node[:openwrt][:freeradius2_ver], node[:openwrt][:freeradius2_md5] = 
  case source_name
  when "trunk"
    ["2.1.12", "862d3a2c11011e61890ba84fa636ed8c"]
  else
    ["2.1.10", "8ea2bd39460a06212decf2c14fdf3fb8"]
  end

%w{subversion gawk unzip libncurses5-dev zlib1g-dev flex gettext git-core }.each do |pkg|
  package pkg do
    action :install
  end
end

template "profile" do
  path "/home/vagrant/.build_profile"
  source "#{target}-profile.erb"
  owner "vagrant"
  group "vagrant"
  mode 0644
end

directory "/usr/src/openwrt" do
  owner "vagrant"
  group "vagrant"
end

subversion source_name do
  repository "svn://svn.openwrt.org/openwrt#{source}"
  destination buildroot
  user "vagrant"
  group "vagrant"
  revision "31182"
  action :sync
end

execute "update feeds" do
  cwd buildroot
  command "scripts/feeds update -a"
  user "vagrant"
  group "vagrant"
  action :run
end

execute "install feeds" do 
  cwd buildroot
  command "scripts/feeds install -a"
  user "vagrant"
  group "vagrant"
  action :run
end

cookbook_file "#{buildroot}/.config" do
  source "#{target}-#{source_name}-config"
  owner "vagrant"
  group "vagrant"
  action :create
end

cookbook_file "#{buildroot}/feeds/packages/net/coova-chilli/Makefile" do
  source "coovachilli-Makefile"
  owner "vagrant"
  group "vagrant"
  action :create
end

template "#{buildroot}/feeds/packages/net/freeradius2/Makefile" do
  source "freeradius2-Makefile.erb"
  owner "vagrant"
  group "vagrant"
  action :create
end

directory "#{buildroot}/package/liboath" do
  owner "vagrant"
  group "vagrant"
end

cookbook_file "#{buildroot}/package/liboath/Makefile" do
  source "liboath-Makefile"
  owner "vagrant"
  group "vagrant"
  action :create
end

execute "make prereq" do
  cwd buildroot
  command "make prereq"
  user "vagrant"
  group "vagrant"
  action :run
end

