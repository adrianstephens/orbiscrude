#include "dxgi_helpers.h"

namespace iso {




com_ptr<IDXGIAdapter1> GetAdapter(int index) {
	UINT createFlags = 0;
#ifdef ISO_DEBUG
	createFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	com_ptr<IDXGIFactory4>	dxgi;
	com_ptr<IDXGIAdapter1>	adapter;
	return SUCCEEDED(CreateDXGIFactory2(createFlags, __uuidof(IDXGIFactory), (void**)&dxgi))
		&& SUCCEEDED(dxgi->EnumAdapters1(index, &adapter))
		? move(adapter)
		: nullptr;
}

#ifndef PLAT_XONE
com_ptr<IDXGIAdapter3> GetAdapter(LUID id) {
	if (!id.HighPart && !id.LowPart)
		return nullptr;

	UINT createFlags = 0;
#ifdef ISO_DEBUG
	createFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	com_ptr<IDXGIFactory1>	dxgi;
	com_ptr<IDXGIAdapter3>	adapter;
	return SUCCEEDED(CreateDXGIFactory2(createFlags, __uuidof(IDXGIFactory1), (void**)&dxgi))
		&& SUCCEEDED(temp_com_cast<IDXGIFactory4>(dxgi)->EnumAdapterByLuid(id, __uuidof(adapter), (void**)&adapter))
		? move(adapter)
		: nullptr;
}
#endif

com_ptr<IDXGIOutput> FindAdapterOutput(char* name) {
	com_ptr<IDXGIFactory>	dxgi;
	CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&dxgi);

	// Iterate through adapters.
	com_ptr<IDXGIAdapter>	adapter;
	for (int i = 0; dxgi->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i, adapter.clear()) {
		// Iterate through outputs.
		com_ptr<IDXGIOutput>	output;
		for (int j = 0; adapter->EnumOutputs(j, &output) != DXGI_ERROR_NOT_FOUND; j++, output.clear()) {
			DXGI_OUTPUT_DESC desc;
			output->GetDesc(&desc);
			if (str(desc.DeviceName).find(name))
				return output;
		}
	}
	return nullptr;
}

com_ptr<IDXGIOutput> GetAdapterOutput(int adapter_index, int output_index) {
	com_ptr<IDXGIFactory>	dxgi;
	com_ptr<IDXGIAdapter>	adapter;
	com_ptr<IDXGIOutput>	output;
	return SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&dxgi))
		&& SUCCEEDED(dxgi->EnumAdapters(adapter_index, &adapter))
		&& SUCCEEDED(adapter->EnumOutputs(output_index, &output))
		? move(output)
		: nullptr;
}

#ifndef PLAT_XONE
// get the DXGI factory that was used to create the Direct3D device
com_ptr<IDXGIFactory3> GetDXGIFactory(IUnknown *device) {
	com_ptr<IDXGIDevice3> dxgiDevice = com_cast<IDXGIDevice3>(device);
	com_ptr<IDXGIAdapter> dxgiAdapter;
	dxgiDevice->GetAdapter(&dxgiAdapter);

	IDXGIFactory3	*dxgi;
	dxgiAdapter->GetParent(COM_CREATE(&dxgi));
	return dxgi;
}
#endif


} // namespace iso

