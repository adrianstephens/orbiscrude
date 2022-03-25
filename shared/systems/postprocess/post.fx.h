struct layout_post {
	iso::pointer32<technique> colour;
	iso::pointer32<technique> brightpass;
	iso::pointer32<technique> cubemapface_exp;
	iso::pointer32<technique> cubemapface;
	iso::pointer32<technique> composition;
	iso::pointer32<technique> depth_mip;
	iso::pointer32<technique> zcopy;
	iso::pointer32<technique> zdownres;
	iso::pointer32<technique> downsample4x4;
	iso::pointer32<technique> copy;
	iso::pointer32<technique> adjust;
	iso::pointer32<technique> convolve;
	iso::pointer32<technique> bilateral;
	iso::pointer32<technique> msaa2to1copy;
	iso::pointer32<technique> msaa4to1copy;
};
