#include "H5Cpp.h"
#include "params.h"
#include "fileIO.h"
#include "utility.h"
#include <mutex>

namespace Prismatic{

std::mutex write4D_lock;

void setupOutputFile(Parameters<PRISMATIC_FLOAT_PRECISION> &pars)
{
	//create main groups
	H5::Group simulation(pars.outputFile.createGroup("/4DSTEM_simulation"));

	//set version attributes
	int maj_data = 0;
	int min_data = 5;
	int group_type = 2;

	H5::DataSpace attr_dataspace(H5S_SCALAR);

	H5::Attribute maj_attr = simulation.createAttribute("version_major", H5::PredType::NATIVE_INT, attr_dataspace);
	H5::Attribute min_attr = simulation.createAttribute("version_minor", H5::PredType::NATIVE_INT, attr_dataspace);
	H5::Attribute emd_group_type_attr = simulation.createAttribute("emd_group_type", H5::PredType::NATIVE_INT, attr_dataspace);

	maj_attr.write(H5::PredType::NATIVE_INT, &maj_data);
	min_attr.write(H5::PredType::NATIVE_INT, &min_data);
	emd_group_type_attr.write(H5::PredType::NATIVE_INT, &group_type);

	//data groups
	H5::Group data(simulation.createGroup("data"));
	H5::Group datacubes(data.createGroup("datacubes"));
	H5::Group dslices(data.createGroup("diffractionslices"));
	H5::Group rslices(data.createGroup("realslices"));
	H5::Group pointlists(data.createGroup("pointlists"));	//point lists and point list arrays are not used in prismatic
	H5::Group plarrays(data.createGroup("pointlistarrays")); //included here to maintain consistency with format
	H5::Group supergroups(data.createGroup("supergroups"));

	//log group
	H5::Group log(simulation.createGroup("log"));

	//metadata groups
	H5::Group metadata(simulation.createGroup("metadata"));
	H5::Group metadata_0(metadata.createGroup("metadata_0")); //for consistency with py4DSTEM v0.4

	H5::Group original(metadata_0.createGroup("original"));
	H5::Group shortlist(original.createGroup("shortlist"));
	H5::Group all(original.createGroup("all"));
	H5::Group microscope(metadata_0.createGroup("microscope"));
	H5::Group sample(metadata_0.createGroup("sample"));
	H5::Group user(metadata_0.createGroup("user"));
	H5::Group calibration(metadata_0.createGroup("calibration"));
	H5::Group comments(metadata_0.createGroup("comments"));
}

void setup4DOutput(Parameters<PRISMATIC_FLOAT_PRECISION> &pars, const size_t numLayers)
{
	H5::Group datacubes = pars.outputFile.openGroup("4DSTEM_simulation/data/datacubes");

	//shared properties
	std::string base_name = "CBED_array_depth";
	hsize_t attr_dims[1] = {1};
	hsize_t data_dims[4];
	data_dims[0] = {pars.xp.size()};
	data_dims[1] = {pars.yp.size()};
	hsize_t chunkDims[4];
	chunkDims[0] = chunkDims[1] = {1};
	hsize_t rx_dim[1] = {pars.xp.size()};
	hsize_t ry_dim[1] = {pars.yp.size()};
	hsize_t qx_dim[1];
	hsize_t qy_dim[1];

	Array1D<PRISMATIC_FLOAT_PRECISION> qx;
	Array1D<PRISMATIC_FLOAT_PRECISION> qy;
	long offset_qx;
	long offset_qy;

    size_t qxInd_max = 0;
    size_t qyInd_max = 0;

    if(pars.meta.crop4DOutput)
    {

        PRISMATIC_FLOAT_PRECISION qMax = pars.meta.crop4Damax / pars.lambda;

        for(auto i = 0; i < pars.qx.get_dimi(); i++)
        {
            if(pars.qx.at(i) < qMax)
            {
                qxInd_max++;
            }
            else
            {
                break;
            }
        }

        for(auto j = 0; j < pars.qy.get_dimi(); j++)
        {
            if(pars.qy.at(j) < qMax)
            {
                qyInd_max++;
            }
            else
            {
                break;
            }
        }

        qxInd_max *= 2;
        qyInd_max *= 2;
        //TODO: Figure out how to correctly sit the dimension offset for a cropped image
        offset_qx = 0;
        offset_qy = 0;
    }
    else
    {
        if (pars.meta.algorithm == Algorithm::Multislice)
        {
            qxInd_max = pars.psiProbeInit.get_dimi() / 2;
            qyInd_max = pars.psiProbeInit.get_dimj() / 2;
            offset_qx = pars.psiProbeInit.get_dimi() / 4;
            offset_qy = pars.psiProbeInit.get_dimj() / 4;
        }
        else
        {
            qxInd_max = pars.qx.get_dimi();
            qyInd_max = pars.qy.get_dimi();
            offset_qx = 0;
            offset_qy = 0;
        }
    }

	if (pars.meta.algorithm == Algorithm::Multislice)
	{
		data_dims[2] = {qxInd_max};
		data_dims[3] = {qyInd_max};
		qx_dim[0] = {qxInd_max};
		qy_dim[0] = {qyInd_max};
		qx = fftshift(pars.qx);
		qy = fftshift(pars.qy);
		chunkDims[2] = {qxInd_max};
		chunkDims[3] = {qyInd_max};
	}
	else
	{
		data_dims[2] = {qxInd_max};
		data_dims[3] = {qyInd_max};
		qx_dim[0] = {qxInd_max};
		qy_dim[0] = {qyInd_max};
		qx = pars.qx;
		qy = pars.qy;
		chunkDims[2] = {qxInd_max};
		chunkDims[3] = {qyInd_max};
	}

	for (auto n = 0; n < numLayers; n++)
	{
		//create slice group
		std::string nth_name = base_name + getDigitString(n);
		H5::Group CBED_slice_n(datacubes.createGroup(nth_name.c_str()));

		//write group type attribute
		H5::DataSpace attr1_dataspace(H5S_SCALAR);
		H5::Attribute emd_group_type = CBED_slice_n.createAttribute("emd_group_type", H5::PredType::NATIVE_INT, attr1_dataspace);
		int group_type = 1;
		emd_group_type.write(H5::PredType::NATIVE_INT, &group_type);

		//write metadata attribute
		H5::DataSpace attr2_dataspace(H5S_SCALAR);
		H5::Attribute metadata_group = CBED_slice_n.createAttribute("metadata", H5::PredType::NATIVE_INT, attr2_dataspace);
		int mgroup = 0;
		metadata_group.write(H5::PredType::NATIVE_INT, &mgroup);

		//write output depth attribute
		H5::DataSpace attr3_dataspace(H5S_SCALAR);
		H5::Attribute output_depth = CBED_slice_n.createAttribute("output_depth", PFP_TYPE, attr3_dataspace);
		output_depth.write(PFP_TYPE, &pars.depths[n]);
		
		//setup data set chunking properties
		H5::DSetCreatPropList plist;
		plist.setChunk(4, chunkDims);

		//create dataset
		H5::DataSpace mspace(4, data_dims); //rank is 4
		H5::DataSet CBED_data = CBED_slice_n.createDataSet("datacube", PFP_TYPE, mspace, plist);
		mspace.close();

		//write dimensions
		H5::DataSpace str_name_ds(H5S_SCALAR);
		H5::StrType strdatatype(H5::PredType::C_S1, 256);

		H5::DataSpace dim1_mspace(1, rx_dim);
		H5::DataSpace dim2_mspace(1, ry_dim);
		H5::DataSpace dim3_mspace(1, qx_dim);
		H5::DataSpace dim4_mspace(1, qy_dim);

		H5::DataSet dim1 = CBED_slice_n.createDataSet("dim1", PFP_TYPE, dim1_mspace);
		H5::DataSet dim2 = CBED_slice_n.createDataSet("dim2", PFP_TYPE, dim2_mspace);
		H5::DataSet dim3 = CBED_slice_n.createDataSet("dim3", PFP_TYPE, dim3_mspace);
		H5::DataSet dim4 = CBED_slice_n.createDataSet("dim4", PFP_TYPE, dim4_mspace);

		H5::DataSpace dim1_fspace = dim1.getSpace();
		H5::DataSpace dim2_fspace = dim2.getSpace();
		H5::DataSpace dim3_fspace = dim3.getSpace();
		H5::DataSpace dim4_fspace = dim4.getSpace();

		dim1.write(&pars.xp[0], PFP_TYPE, dim1_mspace, dim1_fspace);
		dim2.write(&pars.yp[0], PFP_TYPE, dim2_mspace, dim2_fspace);
		dim3.write(&qx[offset_qx], PFP_TYPE, dim3_mspace, dim3_fspace);
		dim4.write(&qy[offset_qy], PFP_TYPE, dim4_mspace, dim4_fspace);

		//dimension attributes
		const H5std_string dim1_name_str("R_x");
		const H5std_string dim2_name_str("R_y");
		const H5std_string dim3_name_str("Q_x");
		const H5std_string dim4_name_str("Q_y");

		H5::Attribute dim1_name = dim1.createAttribute("name", strdatatype, str_name_ds);
		H5::Attribute dim2_name = dim2.createAttribute("name", strdatatype, str_name_ds);
		H5::Attribute dim3_name = dim3.createAttribute("name", strdatatype, str_name_ds);
		H5::Attribute dim4_name = dim4.createAttribute("name", strdatatype, str_name_ds);

		dim1_name.write(strdatatype, dim1_name_str);
		dim2_name.write(strdatatype, dim2_name_str);
		dim3_name.write(strdatatype, dim3_name_str);
		dim4_name.write(strdatatype, dim4_name_str);

		const H5std_string dim1_unit_str("[n_m]");
		const H5std_string dim2_unit_str("[n_m]");
		const H5std_string dim3_unit_str("[n_m^-1]");
		const H5std_string dim4_unit_str("[n_m^-1]");

		H5::Attribute dim1_unit = dim1.createAttribute("units", strdatatype, str_name_ds);
		H5::Attribute dim2_unit = dim2.createAttribute("units", strdatatype, str_name_ds);
		H5::Attribute dim3_unit = dim3.createAttribute("units", strdatatype, str_name_ds);
		H5::Attribute dim4_unit = dim4.createAttribute("units", strdatatype, str_name_ds);

		dim1_unit.write(strdatatype, dim1_unit_str);
		dim2_unit.write(strdatatype, dim2_unit_str);
		dim3_unit.write(strdatatype, dim3_unit_str);
		dim4_unit.write(strdatatype, dim4_unit_str);

		CBED_slice_n.close();
	}

	datacubes.close();
};

void setupVDOutput(Parameters<PRISMATIC_FLOAT_PRECISION> &pars, const size_t numLayers)
{
	H5::Group realslices = pars.outputFile.openGroup("4DSTEM_simulation/data/realslices");

	//shared properties
	std::string base_name = "virtual_detector_depth";
	hsize_t attr_dims[1] = {1};
	hsize_t data_dims[3];
	data_dims[0] = {pars.xp.size()};
	data_dims[1] = {pars.yp.size()};
	data_dims[2] = {pars.Ndet};

	hsize_t rx_dim[1] = {pars.xp.size()};
	hsize_t ry_dim[1] = {pars.yp.size()};
	hsize_t bin_dim[1] = {pars.Ndet};

	for (auto n = 0; n < numLayers; n++)
	{
		//create slice group
		std::string nth_name = base_name + getDigitString(n);
		H5::Group VD_slice_n(realslices.createGroup(nth_name.c_str()));

		//write group type attribute
		H5::DataSpace attr1_dataspace(H5S_SCALAR);
		H5::Attribute emd_group_type = VD_slice_n.createAttribute("emd_group_type", H5::PredType::NATIVE_INT, attr1_dataspace);
		int group_type = 1;
		emd_group_type.write(H5::PredType::NATIVE_INT, &group_type);

		//write metadata attribute
		H5::DataSpace attr2_dataspace(H5S_SCALAR);
		H5::Attribute metadata_group = VD_slice_n.createAttribute("metadata", H5::PredType::NATIVE_INT, attr2_dataspace);
		int mgroup = 0;
		metadata_group.write(H5::PredType::NATIVE_INT, &mgroup);

		//write output depth attribute
		H5::DataSpace attr3_dataspace(H5S_SCALAR);
		H5::Attribute output_depth = VD_slice_n.createAttribute("output_depth", PFP_TYPE, attr3_dataspace);
		output_depth.write(PFP_TYPE, &pars.depths[n]);

		//create datasets
		H5::DataSpace mspace(3, data_dims); //rank is 2 for each realslice
		H5::DataSet VD_data = VD_slice_n.createDataSet("realslice", PFP_TYPE, mspace);
		VD_data.close();
		mspace.close();

		//write dimensions
		H5::DataSpace str_name_ds(H5S_SCALAR);
		H5::StrType strdatatype(H5::PredType::C_S1, 256);

		H5::DataSpace dim1_mspace(1, rx_dim);
		H5::DataSpace dim2_mspace(1, ry_dim);
		H5::DataSpace dim3_mspace(1, bin_dim);

		H5::DataSet dim1 = VD_slice_n.createDataSet("dim1", PFP_TYPE, dim1_mspace);
		H5::DataSet dim2 = VD_slice_n.createDataSet("dim2", PFP_TYPE, dim2_mspace);
		H5::DataSet dim3 = VD_slice_n.createDataSet("dim3", PFP_TYPE, dim3_mspace);

		H5::DataSpace dim1_fspace = dim1.getSpace();
		H5::DataSpace dim2_fspace = dim2.getSpace();
		H5::DataSpace dim3_fspace = dim3.getSpace();

		dim1.write(&pars.xp[0], PFP_TYPE, dim1_mspace, dim1_fspace);
		dim2.write(&pars.yp[0], PFP_TYPE, dim2_mspace, dim2_fspace);
		dim3.write(&pars.detectorAngles[0], PFP_TYPE, dim3_mspace, dim3_fspace);
		//dimension attributes
		const H5std_string dim1_name_str("R_x");
		const H5std_string dim2_name_str("R_y");
		const H5std_string dim3_name_str("bin_outer_angle");

		H5::Attribute dim1_name = dim1.createAttribute("name", strdatatype, str_name_ds);
		H5::Attribute dim2_name = dim2.createAttribute("name", strdatatype, str_name_ds);
		H5::Attribute dim3_name = dim3.createAttribute("name", strdatatype, str_name_ds);

		dim1_name.write(strdatatype, dim1_name_str);
		dim2_name.write(strdatatype, dim2_name_str);
		dim3_name.write(strdatatype, dim3_name_str);

		const H5std_string dim1_unit_str("[n_m]");
		const H5std_string dim2_unit_str("[n_m]");
		const H5std_string dim3_unit_str("[mrad]");

		H5::Attribute dim1_unit = dim1.createAttribute("units", strdatatype, str_name_ds);
		H5::Attribute dim2_unit = dim2.createAttribute("units", strdatatype, str_name_ds);
		H5::Attribute dim3_unit = dim3.createAttribute("units", strdatatype, str_name_ds);

		dim1_unit.write(strdatatype, dim1_unit_str);
		dim2_unit.write(strdatatype, dim2_unit_str);
		dim3_unit.write(strdatatype, dim3_unit_str);

		VD_slice_n.close();
	}

	realslices.close();
};

void setup2DOutput(Parameters<PRISMATIC_FLOAT_PRECISION> &pars, const size_t numLayers)
{
	H5::Group realslices = pars.outputFile.openGroup("4DSTEM_simulation/data/realslices");

	//shared properties
	std::string base_name = "annular_detector_depth";
	hsize_t attr_dims[1] = {1};
	hsize_t data_dims[2];
	data_dims[0] = {pars.xp.size()};
	data_dims[1] = {pars.yp.size()};

	hsize_t rx_dim[1] = {pars.xp.size()};
	hsize_t ry_dim[1] = {pars.yp.size()};

	for (auto n = 0; n < numLayers; n++)
	{
		//create slice group
		std::string nth_name = base_name + getDigitString(n);
		H5::Group annular_slice_n(realslices.createGroup(nth_name.c_str()));

		//write group type attribute
		H5::DataSpace attr1_dataspace(H5S_SCALAR);
		H5::Attribute emd_group_type = annular_slice_n.createAttribute("emd_group_type", H5::PredType::NATIVE_INT, attr1_dataspace);
		int group_type = 1;
		emd_group_type.write(H5::PredType::NATIVE_INT, &group_type);

		//write metadata attribute
		H5::DataSpace attr2_dataspace(H5S_SCALAR);
		H5::Attribute metadata_group = annular_slice_n.createAttribute("metadata", H5::PredType::NATIVE_INT, attr2_dataspace);
		int mgroup = 0;
		metadata_group.write(H5::PredType::NATIVE_INT, &mgroup);

		//write realslice depth attribute
		int depth = 1;
		H5::DataSpace attr3_dataspace(H5S_SCALAR);
		H5::Attribute depth_attr = annular_slice_n.createAttribute("depth", H5::PredType::NATIVE_INT, attr3_dataspace);
		depth_attr.write(H5::PredType::NATIVE_INT, &depth);

		//write output depth attribute
		H5::DataSpace attr4_dataspace(H5S_SCALAR);
		H5::Attribute output_depth = annular_slice_n.createAttribute("output_depth", PFP_TYPE, attr4_dataspace);
		output_depth.write(PFP_TYPE, &pars.depths[n]);

		//create dataset
		H5::DataSpace mspace(2, data_dims); //rank is 2
		H5::DataSet CBED_data = annular_slice_n.createDataSet("realslice", PFP_TYPE, mspace);
		mspace.close();

		//write dimensions
		H5::DataSpace str_name_ds(H5S_SCALAR);
		H5::StrType strdatatype(H5::PredType::C_S1, 256);

		H5::DataSpace dim1_mspace(1, rx_dim);
		H5::DataSpace dim2_mspace(1, ry_dim);

		H5::DataSet dim1 = annular_slice_n.createDataSet("dim1", PFP_TYPE, dim1_mspace);
		H5::DataSet dim2 = annular_slice_n.createDataSet("dim2", PFP_TYPE, dim2_mspace);

		H5::DataSpace dim1_fspace = dim1.getSpace();
		H5::DataSpace dim2_fspace = dim2.getSpace();

		dim1.write(&pars.xp[0], PFP_TYPE, dim1_mspace, dim1_fspace);
		dim2.write(&pars.yp[0], PFP_TYPE, dim2_mspace, dim2_fspace);

		//dimension attributes
		const H5std_string dim1_name_str("R_x");
		const H5std_string dim2_name_str("R_y");

		H5::Attribute dim1_name = dim1.createAttribute("name", strdatatype, str_name_ds);
		H5::Attribute dim2_name = dim2.createAttribute("name", strdatatype, str_name_ds);

		dim1_name.write(strdatatype, dim1_name_str);
		dim2_name.write(strdatatype, dim2_name_str);

		const H5std_string dim1_unit_str("[n_m]");
		const H5std_string dim2_unit_str("[n_m]");

		H5::Attribute dim1_unit = dim1.createAttribute("units", strdatatype, str_name_ds);
		H5::Attribute dim2_unit = dim2.createAttribute("units", strdatatype, str_name_ds);

		dim1_unit.write(strdatatype, dim1_unit_str);
		dim2_unit.write(strdatatype, dim2_unit_str);

		annular_slice_n.close();
	}

	realslices.close();
};

void setupDPCOutput(Parameters<PRISMATIC_FLOAT_PRECISION> &pars, const size_t numLayers)
{
	H5::Group realslices = pars.outputFile.openGroup("4DSTEM_simulation/data/realslices");

	//shared properties
	std::string base_name = "DPC_CoM_depth";
	hsize_t attr_dims[1] = {1};
	hsize_t data_dims[3];
	data_dims[0] = {pars.xp.size()};
	data_dims[1] = {pars.yp.size()};
	data_dims[2] = {2};

	hsize_t rx_dim[1] = {pars.xp.size()};
	hsize_t ry_dim[1] = {pars.yp.size()};
	hsize_t str_dim[1] = {2};

	for (auto n = 0; n < numLayers; n++)
	{
		//create slice group
		std::string nth_name = base_name + getDigitString(n);
		H5::Group DPC_CoM_slice_n(realslices.createGroup(nth_name.c_str()));

		//write group type attribute
		H5::DataSpace attr1_dataspace(H5S_SCALAR);
		H5::Attribute emd_group_type = DPC_CoM_slice_n.createAttribute("emd_group_type", H5::PredType::NATIVE_INT, attr1_dataspace);
		int group_type = 1;
		emd_group_type.write(H5::PredType::NATIVE_INT, &group_type);

		//write metadata attribute
		H5::DataSpace attr2_dataspace(H5S_SCALAR);
		H5::Attribute metadata_group = DPC_CoM_slice_n.createAttribute("metadata", H5::PredType::NATIVE_INT, attr2_dataspace);
		int mgroup = 0;
		metadata_group.write(H5::PredType::NATIVE_INT, &mgroup);

		//write output depth attribute
		H5::DataSpace attr3_dataspace(H5S_SCALAR);
		H5::Attribute output_depth = DPC_CoM_slice_n.createAttribute("output_depth", PFP_TYPE, attr3_dataspace);
		output_depth.write(PFP_TYPE, &pars.depths[n]);

		//create dataset
		H5::DataSpace mspace(3, data_dims); //rank is 3
		H5::DataSet DPC_data = DPC_CoM_slice_n.createDataSet("realslice", PFP_TYPE, mspace);
		mspace.close();

		//write dimensions
		H5::DataSpace str_name_ds(H5S_SCALAR);
		H5::StrType strdatatype(H5::PredType::C_S1, 256);

		H5::DataSpace dim1_mspace(1, rx_dim);
		H5::DataSpace dim2_mspace(1, ry_dim);
		H5::DataSpace dim3_mspace(1, str_dim);

		H5::DataSet dim1 = DPC_CoM_slice_n.createDataSet("dim1", PFP_TYPE, dim1_mspace);
		H5::DataSet dim2 = DPC_CoM_slice_n.createDataSet("dim2", PFP_TYPE, dim2_mspace);

		H5::DataSpace dim1_fspace = dim1.getSpace();
		H5::DataSpace dim2_fspace = dim2.getSpace();

		dim1.write(&pars.xp[0], PFP_TYPE, dim1_mspace, dim1_fspace);
		dim2.write(&pars.yp[0], PFP_TYPE, dim2_mspace, dim2_fspace);

		H5::DataSet dim3 = DPC_CoM_slice_n.createDataSet("dim3", strdatatype, dim3_mspace);
		H5std_string dpc_x("DPC_CoM_x");
		H5std_string dpc_y("DPC_CoM_y");
		H5std_string str_buffer_array[2];
		str_buffer_array[0] = dpc_x;
		str_buffer_array[1] = dpc_y;

		writeStringArray(dim3, str_buffer_array, 2);

		//dimension attributes
		const H5std_string dim1_name_str("R_x");
		const H5std_string dim2_name_str("R_y");

		H5::Attribute dim1_name = dim1.createAttribute("name", strdatatype, str_name_ds);
		H5::Attribute dim2_name = dim2.createAttribute("name", strdatatype, str_name_ds);

		dim1_name.write(strdatatype, dim1_name_str);
		dim2_name.write(strdatatype, dim2_name_str);

		const H5std_string dim1_unit_str("[n_m]");
		const H5std_string dim2_unit_str("[n_m]");

		H5::Attribute dim1_unit = dim1.createAttribute("units", strdatatype, str_name_ds);
		H5::Attribute dim2_unit = dim2.createAttribute("units", strdatatype, str_name_ds);

		dim1_unit.write(strdatatype, dim1_unit_str);
		dim2_unit.write(strdatatype, dim2_unit_str);

		DPC_CoM_slice_n.close();
	}

	realslices.close();
};

void setupSMatrixOutput(Parameters<PRISMATIC_FLOAT_PRECISION> &pars, const int FP)
{
	H5::Group realslices = pars.outputFile.openGroup("4DSTEM_simulation/data/realslices");

	std::string base_name = "smatrix_fp" + getDigitString(FP);
	hsize_t attr_dims[1] = {1};
	hsize_t data_dims[3] = {pars.Scompact.get_dimi(), pars.Scompact.get_dimj(), pars.Scompact.get_dimk()};

	hsize_t rx_dim[1] = {pars.xp.size()}; //TODO: clarify 
	hsize_t ry_dim[1] = {pars.yp.size()};
	hsize_t beams[1] = {pars.numberBeams};

	H5::CompType complex_type = H5::CompType(sizeof(complex_float_t));
	const H5std_string re_str("r"); //using h5py default configuration
	const H5std_string im_str("i");
	complex_type.insertMember(re_str, 0, PFP_TYPE);
	complex_type.insertMember(im_str, 4, PFP_TYPE);

	H5::Group smatrix_group(realslices.createGroup(base_name.c_str()));

	//write group type attribute
	H5::DataSpace attr1_dataspace(H5S_SCALAR);
	H5::Attribute emd_group_type = smatrix_group.createAttribute("emd_group_type", H5::PredType::NATIVE_INT, attr1_dataspace);
	int group_type = 1;
	emd_group_type.write(H5::PredType::NATIVE_INT, &group_type);

	//write metadata attribute
	H5::DataSpace attr2_dataspace(H5S_SCALAR);
	H5::Attribute metadata_group = smatrix_group.createAttribute("metadata", H5::PredType::NATIVE_INT, attr2_dataspace);
	int mgroup = 0;
	metadata_group.write(H5::PredType::NATIVE_INT, &mgroup);

	//create datasets
	H5::DataSpace mspace(3, data_dims); //rank is 2 for each realslice
	H5::DataSet smatrix_data = smatrix_group.createDataSet("realslice", complex_type, mspace);
	smatrix_data.close();
	mspace.close();

	//write dimensions
	H5::DataSpace str_name_ds(H5S_SCALAR);
	H5::StrType strdatatype(H5::PredType::C_S1, 256);

	H5::DataSpace dim1_mspace(1, rx_dim);
	H5::DataSpace dim2_mspace(1, ry_dim);
	H5::DataSpace dim3_mspace(1, beams);

	H5::DataSet dim1 = smatrix_group.createDataSet("dim1", PFP_TYPE, dim1_mspace);
	H5::DataSet dim2 = smatrix_group.createDataSet("dim2", PFP_TYPE, dim2_mspace);
	H5::DataSet dim3 = smatrix_group.createDataSet("dim3", PFP_TYPE, dim3_mspace);

	H5::DataSpace dim1_fspace = dim1.getSpace();
	H5::DataSpace dim2_fspace = dim2.getSpace();
	H5::DataSpace dim3_fspace = dim3.getSpace();

	// dim1.write(&pars.xp[0], PFP_TYPE, dim1_mspace, dim1_fspace);
	// dim2.write(&pars.yp[0], PFP_TYPE, dim2_mspace, dim2_fspace);
	// dim3.write(&pars.detectorAngles[0], PFP_TYPE, dim3_mspace, dim3_fspace);

	//dimension attributes
	const H5std_string dim1_name_str("R_x");
	const H5std_string dim2_name_str("R_y");
	const H5std_string dim3_name_str("beam_number");

	H5::Attribute dim1_name = dim1.createAttribute("name", strdatatype, str_name_ds);
	H5::Attribute dim2_name = dim2.createAttribute("name", strdatatype, str_name_ds);
	H5::Attribute dim3_name = dim3.createAttribute("name", strdatatype, str_name_ds);

	dim1_name.write(strdatatype, dim1_name_str);
	dim2_name.write(strdatatype, dim2_name_str);
	dim3_name.write(strdatatype, dim3_name_str);

	const H5std_string dim1_unit_str("[Å]");
	const H5std_string dim2_unit_str("[Å]");
	const H5std_string dim3_unit_str("[none]");

	H5::Attribute dim1_unit = dim1.createAttribute("units", strdatatype, str_name_ds);
	H5::Attribute dim2_unit = dim2.createAttribute("units", strdatatype, str_name_ds);
	H5::Attribute dim3_unit = dim3.createAttribute("units", strdatatype, str_name_ds);

	dim1_unit.write(strdatatype, dim1_unit_str);
	dim2_unit.write(strdatatype, dim2_unit_str);
	dim3_unit.write(strdatatype, dim3_unit_str);

	smatrix_group.close();
	realslices.close();

};

void writeRealSlice(H5::DataSet dataset, const PRISMATIC_FLOAT_PRECISION *buffer, const hsize_t *mdims)
{
	H5::DataSpace fspace = dataset.getSpace(); //all realslices have data written all at once
	H5::DataSpace mspace(2, mdims);			   //rank = 2

	dataset.write(buffer, PFP_TYPE, mspace, fspace);

	fspace.close();
	mspace.close();
}

void writeDatacube3D(H5::DataSet dataset, const PRISMATIC_FLOAT_PRECISION *buffer, const hsize_t *mdims)
{
	//set up file and memory spaces
	H5::DataSpace fspace = dataset.getSpace(); //all 3D cubes will write full buffer at once
	H5::DataSpace mspace(3, mdims);			   //rank = 3

	dataset.write(buffer, PFP_TYPE, mspace, fspace);

	fspace.close();
	mspace.close();
};

//for 4D writes, need to first read the data set and then add; this way, FP are accounted for
void writeDatacube4D(Parameters<PRISMATIC_FLOAT_PRECISION> &pars, PRISMATIC_FLOAT_PRECISION *buffer, const hsize_t *mdims, const hsize_t *offset, const PRISMATIC_FLOAT_PRECISION numFP, const std::string nameString)
{
	//lock the whole file access/writing procedure in only one location
	std::unique_lock<std::mutex> writeGatekeeper(write4D_lock);

    H5::Group dataGroup = pars.outputFile.openGroup(nameString);
    H5::DataSet dataset = dataGroup.openDataSet("datacube");

    //set up file and memory spaces
    H5::DataSpace fspace = dataset.getSpace();

    H5::DataSpace mspace(4, mdims); //rank = 4

    fspace.selectHyperslab(H5S_SELECT_SET, mdims, offset);

    //divide by num FP
    for (auto i = 0; i < mdims[0] * mdims[1] * mdims[2] * mdims[3]; i++)
        buffer[i] /= numFP;

    //restride the dataset so that qx and qy are flipped
    PRISMATIC_FLOAT_PRECISION *finalBuffer = (PRISMATIC_FLOAT_PRECISION *)malloc(mdims[0] * mdims[1] * mdims[2] * mdims[3] * sizeof(PRISMATIC_FLOAT_PRECISION));
    for (auto i = 0; i < mdims[2]; i++)
    {
        for (auto j = 0; j < mdims[3]; j++)
        {
            finalBuffer[i * mdims[3] + j] = buffer[j * mdims[2] + i];
        }
    }

    //add frozen phonon set
    PRISMATIC_FLOAT_PRECISION *readBuffer = (PRISMATIC_FLOAT_PRECISION *)malloc(mdims[0] * mdims[1] * mdims[2] * mdims[3] * sizeof(PRISMATIC_FLOAT_PRECISION));
    dataset.read(&readBuffer[0], PFP_TYPE, mspace, fspace);
    for (auto i = 0; i < mdims[0] * mdims[1] * mdims[2] * mdims[3]; i++)
        finalBuffer[i] += readBuffer[i];
    free(readBuffer);
		
    dataset.write(finalBuffer, PFP_TYPE, mspace, fspace);
    free(finalBuffer);
    fspace.close();
    mspace.close();
    dataset.flush(H5F_SCOPE_LOCAL);
    dataset.close();
    dataGroup.flush(H5F_SCOPE_LOCAL);
    dataGroup.close();
    pars.outputFile.flush(H5F_SCOPE_LOCAL);

	writeGatekeeper.unlock();
};

void writeStringArray(H5::DataSet dataset, H5std_string *string_array, const hsize_t elements)
{
	//assumes that we are writing a 1 dimensional array of strings- used pretty much only for DPC
	H5::StrType strdatatype(H5::PredType::C_S1, 256);
	hsize_t offset[1] = {0};
	H5::DataSpace fspace = dataset.getSpace();
	hsize_t str_write_dim[1] = {1};
	H5::DataSpace mspace(1, str_write_dim);

	for (hsize_t i = 0; i < elements; i++)
	{
		offset[0] = {i};
		fspace.selectHyperslab(H5S_SELECT_SET, str_write_dim, offset);
		dataset.write(string_array[i], strdatatype, mspace, fspace);
	}
	fspace.close();
	mspace.close();
}

void savePotentialSlices(Parameters<PRISMATIC_FLOAT_PRECISION> &pars)
{
	// create new datacube group
	H5::Group realslices = pars.outputFile.openGroup("4DSTEM_simulation/data/realslices");
	std::stringstream nameString;
	nameString << "ppotential_fp" << getDigitString(pars.fpFlag);
	std::string groupName = nameString.str();
	H5::Group ppotential;

	ppotential = realslices.createGroup(groupName);

	H5::DataSpace attr_dataspace(H5S_SCALAR);

	int group_type = 1;
	H5::Attribute emd_group_type = ppotential.createAttribute("emd_group_type", H5::PredType::NATIVE_INT, attr_dataspace);
	emd_group_type.write(H5::PredType::NATIVE_INT, &group_type);

	H5::Attribute metadata_group = ppotential.createAttribute("metadata", H5::PredType::NATIVE_INT, attr_dataspace);
	int mgroup = 0;
	metadata_group.write(H5::PredType::NATIVE_INT, &mgroup);

	//write dimensions
	H5::DataSpace str_name_ds(H5S_SCALAR);
	H5::StrType strdatatype(H5::PredType::C_S1, 256);

	hsize_t x_size[1] = {pars.imageSize[1]};
	hsize_t y_size[1] = {pars.imageSize[0]};
	hsize_t z_size[1] = {pars.numPlanes};

	Array1D<PRISMATIC_FLOAT_PRECISION> x_dim_data = zeros_ND<1, PRISMATIC_FLOAT_PRECISION>({{pars.imageSize[1]}});
	Array1D<PRISMATIC_FLOAT_PRECISION> y_dim_data = zeros_ND<1, PRISMATIC_FLOAT_PRECISION>({{pars.imageSize[0]}});
	Array1D<PRISMATIC_FLOAT_PRECISION> z_dim_data = zeros_ND<1, PRISMATIC_FLOAT_PRECISION>({{pars.numPlanes}});

	for (auto i = 0; i < pars.imageSize[1]; i++)
		x_dim_data[i] = i * pars.pixelSize[1];
	for (auto i = 0; i < pars.imageSize[0]; i++)
		y_dim_data[i] = i * pars.pixelSize[0];
	for (auto i = 0; i < pars.numPlanes; i++)
		z_dim_data[i] = i * pars.meta.sliceThickness;

	H5::DataSpace dim1_mspace(1, x_size);
	H5::DataSpace dim2_mspace(1, y_size);
	H5::DataSpace dim3_mspace(1, z_size);

	H5::DataSet dim1;
	H5::DataSet dim2;
	H5::DataSet dim3;

	dim1 = ppotential.createDataSet("dim1", PFP_TYPE, dim1_mspace);
	dim2 = ppotential.createDataSet("dim2", PFP_TYPE, dim2_mspace);
	dim3 = ppotential.createDataSet("dim3", PFP_TYPE, dim3_mspace);

	H5::DataSpace dim1_fspace = dim1.getSpace();
	H5::DataSpace dim2_fspace = dim2.getSpace();
	H5::DataSpace dim3_fspace = dim3.getSpace();

	dim1.write(&x_dim_data[0], PFP_TYPE, dim1_mspace, dim1_fspace);
	dim2.write(&y_dim_data[0], PFP_TYPE, dim2_mspace, dim2_fspace);
	dim3.write(&z_dim_data[0], PFP_TYPE, dim3_mspace, dim3_fspace);

	//dimension attributes
	const H5std_string dim1_name_str("R_x");
	const H5std_string dim2_name_str("R_y");
	const H5std_string dim3_name_str("R_z");

	H5::Attribute dim1_name = dim1.createAttribute("name", strdatatype, str_name_ds);
	H5::Attribute dim2_name = dim2.createAttribute("name", strdatatype, str_name_ds);
	H5::Attribute dim3_name = dim3.createAttribute("name", strdatatype, str_name_ds);

	dim1_name.write(strdatatype, dim1_name_str);
	dim2_name.write(strdatatype, dim2_name_str);
	dim3_name.write(strdatatype, dim3_name_str);

	const H5std_string dim1_unit_str("[n_m]");
	const H5std_string dim2_unit_str("[n_m]");
	const H5std_string dim3_unit_str("[n_m]");

	H5::Attribute dim1_unit = dim1.createAttribute("units", strdatatype, str_name_ds);
	H5::Attribute dim2_unit = dim2.createAttribute("units", strdatatype, str_name_ds);
	H5::Attribute dim3_unit = dim3.createAttribute("units", strdatatype, str_name_ds);

	dim1_unit.write(strdatatype, dim1_unit_str);
	dim2_unit.write(strdatatype, dim2_unit_str);
	dim3_unit.write(strdatatype, dim3_unit_str);

	//read in potential array and re-stride
	//TODO: use restride
	std::array<size_t, 3> dims_in = {pars.imageSize[1], pars.imageSize[0], pars.numPlanes};
	std::array<size_t, 3> order = {2, 1, 0};

	Array3D<PRISMATIC_FLOAT_PRECISION> writeBuffer = restride(pars.pot, dims_in, order);
	
	//  zeros_ND<3, PRISMATIC_FLOAT_PRECISION>({{pars.imageSize[1], pars.imageSize[0], pars.numPlanes}});
	// for (auto x = 0; x < pars.imageSize[1]; x++)
	// {
	// 	for (auto y = 0; y < pars.imageSize[0]; y++)
	// 	{
	// 		for (auto z = 0; z < pars.numPlanes; z++)
	// 		{
	// 			writeBuffer.at(x, y, z) = pars.pot.at(z, y, x);
	// 		}
	// 	}
	// }

	H5::DataSet potSliceData; //declare out here to avoid scoping
	std::string slice_name = "realslice";
	//create dataset
	hsize_t dataDims[3] = {pars.imageSize[1], pars.imageSize[0], pars.numPlanes};
	H5::DataSpace mspace(3, dataDims);

	//switch between float and double, maybe not the best way to do so
	potSliceData = ppotential.createDataSet(slice_name, PFP_TYPE, mspace);

	hsize_t wmdims[3] = {pars.imageSize[1], pars.imageSize[0], pars.numPlanes};
	H5::DataSpace wfspace = potSliceData.getSpace();
	H5::DataSpace wmspace(3, wmdims);

	potSliceData.write(&writeBuffer[0], PFP_TYPE, wmspace, wfspace);

	potSliceData.close();
}

std::string getDigitString(int digit)
{
	char buffer[20];
	sprintf(buffer, "%04d", digit);
	std::string output = buffer;
	return output;
};

void writeMetadata(Parameters<PRISMATIC_FLOAT_PRECISION> &pars)
{
	//set up group
	H5::Group metadata = pars.outputFile.openGroup("4DSTEM_simulation/metadata/metadata_0/original");
	H5::Group sim_params = metadata.createGroup("simulation_parameters");

	//write all parameters as attributes

	//create common dataspaces
	H5::DataSpace str_name_ds(H5S_SCALAR); //string dataspaces and types
	H5::StrType strdatatype(H5::PredType::C_S1, 256);
	H5::DataSpace scalar_attr(H5S_SCALAR);

	//initialize string parameter data
	H5std_string algorithm;
	if (pars.meta.algorithm == Algorithm::Multislice)
	{
		algorithm = "m";
	}
	else
	{
		algorithm = "p";
	}

	const H5std_string filenameAtoms(pars.meta.filenameAtoms);

	//create string attributes
	H5::Attribute atoms_attr = sim_params.createAttribute("i", strdatatype, str_name_ds);
	H5::Attribute alg_attr = sim_params.createAttribute("a", strdatatype, str_name_ds);

	//create scalar logical/integer attributes
	H5::Attribute fx_attr = sim_params.createAttribute("fx", H5::PredType::NATIVE_INT, scalar_attr);
	H5::Attribute fy_attr = sim_params.createAttribute("fy", H5::PredType::NATIVE_INT, scalar_attr);
	H5::Attribute numFP_attr = sim_params.createAttribute("F", H5::PredType::NATIVE_INT, scalar_attr);
	H5::Attribute numSlices_attr = sim_params.createAttribute("ns", H5::PredType::NATIVE_INT, scalar_attr);
	H5::Attribute te_attr = sim_params.createAttribute("te", H5::PredType::NATIVE_INT, scalar_attr);
	H5::Attribute oc_attr = sim_params.createAttribute("oc", H5::PredType::NATIVE_INT, scalar_attr);
	H5::Attribute save3D_attr = sim_params.createAttribute("3D", H5::PredType::NATIVE_INT, scalar_attr);
	H5::Attribute save4D_attr = sim_params.createAttribute("4D", H5::PredType::NATIVE_INT, scalar_attr);
	H5::Attribute saveDPC_attr = sim_params.createAttribute("DPC", H5::PredType::NATIVE_INT, scalar_attr);
	H5::Attribute savePS_attr = sim_params.createAttribute("ps", H5::PredType::NATIVE_INT, scalar_attr);
	H5::Attribute nyquist_attr = sim_params.createAttribute("nqs", H5::PredType::NATIVE_INT, scalar_attr);

	//create scalar float/double attributes (changes based on prismatic float precision)
	H5::Attribute px_attr = sim_params.createAttribute("px", PFP_TYPE, scalar_attr);
	H5::Attribute py_attr = sim_params.createAttribute("py", PFP_TYPE, scalar_attr);
	H5::Attribute potBound_attr = sim_params.createAttribute("P", PFP_TYPE, scalar_attr);
	H5::Attribute sliceThickness_attr = sim_params.createAttribute("s", PFP_TYPE, scalar_attr);
	H5::Attribute zStart_attr = sim_params.createAttribute("zs", PFP_TYPE, scalar_attr);
	H5::Attribute E0_attr = sim_params.createAttribute("E", PFP_TYPE, scalar_attr);
	H5::Attribute alphaMax_attr = sim_params.createAttribute("A", PFP_TYPE, scalar_attr);
	H5::Attribute rx_attr = sim_params.createAttribute("rx", PFP_TYPE, scalar_attr);
	H5::Attribute ry_attr = sim_params.createAttribute("ry", PFP_TYPE, scalar_attr);
	H5::Attribute df_attr = sim_params.createAttribute("df", PFP_TYPE, scalar_attr);
	H5::Attribute C3_attr = sim_params.createAttribute("C3", PFP_TYPE, scalar_attr);
	H5::Attribute C5_attr = sim_params.createAttribute("C5", PFP_TYPE, scalar_attr);
	H5::Attribute semiangle_attr = sim_params.createAttribute("sa", PFP_TYPE, scalar_attr);
	H5::Attribute detector_attr = sim_params.createAttribute("d", PFP_TYPE, scalar_attr);
	H5::Attribute tx_attr = sim_params.createAttribute("tx", PFP_TYPE, scalar_attr);
	H5::Attribute ty_attr = sim_params.createAttribute("ty", PFP_TYPE, scalar_attr);

	//create vector spaces
	hsize_t two[1] = {2};
	hsize_t three[1] = {3};
	H5::DataSpace v_two_dataspace(1, two);
	H5::DataSpace v_three_dataspace(1, three);

	H5::Attribute cell_dim_attr = sim_params.createAttribute("c", PFP_TYPE, v_three_dataspace);
	H5::Attribute tile_attr = sim_params.createAttribute("t", PFP_TYPE, v_three_dataspace);
	H5::Attribute scanWindow_x_attr = sim_params.createAttribute("wx", PFP_TYPE, v_two_dataspace);
	H5::Attribute scanWindow_y_attr = sim_params.createAttribute("wy", PFP_TYPE, v_two_dataspace);

	H5::Attribute scanWindow_x_r_attr;
	H5::Attribute scanWindow_y_r_attr;
	if (pars.meta.realSpaceWindow_x)
		scanWindow_x_r_attr = sim_params.createAttribute("wxr", PFP_TYPE, v_two_dataspace);
	if (pars.meta.realSpaceWindow_y)
		scanWindow_y_r_attr = sim_params.createAttribute("wyr", PFP_TYPE, v_two_dataspace);

	H5::Attribute save2D_attr;
	if (pars.meta.save2DOutput)
	{
		save2D_attr = sim_params.createAttribute("2D", PFP_TYPE, v_two_dataspace);
	}

	//write data
	//strings
	atoms_attr.write(strdatatype, filenameAtoms);
	alg_attr.write(strdatatype, algorithm);

	//scalar logical/integers
	fx_attr.write(H5::PredType::NATIVE_INT, &pars.meta.interpolationFactorX);
	fy_attr.write(H5::PredType::NATIVE_INT, &pars.meta.interpolationFactorY);
	numFP_attr.write(H5::PredType::NATIVE_INT, &pars.meta.numFP);
	numSlices_attr.write(H5::PredType::NATIVE_INT, &pars.meta.numSlices);

	//logicals first need to be cast to ints
	int tmp_te = {pars.meta.includeThermalEffects};
	int tmp_oc = {pars.meta.includeOccupancy};
	int tmp_3D = {pars.meta.save3DOutput};
	int tmp_4D = {pars.meta.save4DOutput};
	int tmp_DPC = {pars.meta.saveDPC_CoM};
	int tmp_PS = {pars.meta.savePotentialSlices};
	int tmp_nqs = {pars.meta.nyquistSampling};

	te_attr.write(H5::PredType::NATIVE_INT, &tmp_te);
	oc_attr.write(H5::PredType::NATIVE_INT, &tmp_oc);
	save3D_attr.write(H5::PredType::NATIVE_INT, &tmp_3D);
	save4D_attr.write(H5::PredType::NATIVE_INT, &tmp_4D);
	saveDPC_attr.write(H5::PredType::NATIVE_INT, &tmp_DPC);
	savePS_attr.write(H5::PredType::NATIVE_INT, &tmp_PS);
	nyquist_attr.write(H5::PredType::NATIVE_INT, &tmp_nqs);

	//scalar floats/doubles
	px_attr.write(PFP_TYPE, &pars.meta.realspacePixelSize[1]);
	py_attr.write(PFP_TYPE, &pars.meta.realspacePixelSize[0]);
	potBound_attr.write(PFP_TYPE, &pars.meta.potBound);
	sliceThickness_attr.write(PFP_TYPE, &pars.meta.sliceThickness);
	zStart_attr.write(PFP_TYPE, &pars.meta.zStart);
	rx_attr.write(PFP_TYPE, &pars.meta.probeStepX);
	ry_attr.write(PFP_TYPE, &pars.meta.probeStepY);
	df_attr.write(PFP_TYPE, &pars.meta.probeDefocus);
	C3_attr.write(PFP_TYPE, &pars.meta.C3);
	C5_attr.write(PFP_TYPE, &pars.meta.C5);

	//scalars with unit adjustments
	PRISMATIC_FLOAT_PRECISION tmp_tx[1] = {pars.meta.probeXtilt * 1000};
	PRISMATIC_FLOAT_PRECISION tmp_ty[1] = {pars.meta.probeYtilt * 1000};
	PRISMATIC_FLOAT_PRECISION tmp_E0[1] = {pars.meta.E0 / 1000};
	PRISMATIC_FLOAT_PRECISION tmp_alphaMax[1] = {pars.meta.alphaBeamMax * 1000};
	PRISMATIC_FLOAT_PRECISION tmp_sa[1] = {pars.meta.probeSemiangle * 1000};
	PRISMATIC_FLOAT_PRECISION tmp_d[1] = {pars.meta.detectorAngleStep * 1000};

	tx_attr.write(PFP_TYPE, &tmp_tx);
	ty_attr.write(PFP_TYPE, &tmp_ty);
	E0_attr.write(PFP_TYPE, &tmp_E0);
	alphaMax_attr.write(PFP_TYPE, &tmp_alphaMax);
	semiangle_attr.write(PFP_TYPE, &tmp_sa);
	detector_attr.write(PFP_TYPE, &tmp_d);

	//vector spaces
	PRISMATIC_FLOAT_PRECISION tmp_buffer[2];

	if (pars.meta.save2DOutput)
	{
		tmp_buffer[0] = pars.meta.integrationAngleMin * 1000;
		tmp_buffer[1] = pars.meta.integrationAngleMax * 1000;
		save2D_attr.write(PFP_TYPE, tmp_buffer);
	}

	if (pars.meta.realSpaceWindow_x)
	{
		tmp_buffer[0] = pars.meta.scanWindowXMin_r;
		tmp_buffer[1] = pars.meta.scanWindowXMax_r;
		scanWindow_x_r_attr.write(PFP_TYPE, tmp_buffer);
	}

	if (pars.meta.realSpaceWindow_y)
	{
		tmp_buffer[0] = pars.meta.scanWindowYMin_r;
		tmp_buffer[1] = pars.meta.scanWindowYMax_r;
		scanWindow_y_r_attr.write(PFP_TYPE, tmp_buffer);
	}

	tmp_buffer[0] = pars.meta.scanWindowXMin;
	tmp_buffer[1] = pars.meta.scanWindowXMax;
	scanWindow_x_attr.write(PFP_TYPE, tmp_buffer);

	tmp_buffer[0] = pars.meta.scanWindowYMin;
	tmp_buffer[1] = pars.meta.scanWindowYMax;
	scanWindow_y_attr.write(PFP_TYPE, tmp_buffer);

	int tile_buffer[3];
	tile_buffer[0] = pars.meta.tileX;
	tile_buffer[1] = pars.meta.tileY;
	tile_buffer[2] = pars.meta.tileZ;
	tile_attr.write(H5::PredType::NATIVE_INT, tile_buffer);

	PRISMATIC_FLOAT_PRECISION cellBuffer[3];
	cellBuffer[0] = pars.meta.cellDim[0];
	cellBuffer[1] = pars.meta.cellDim[1];
	cellBuffer[2] = pars.meta.cellDim[2];
	cell_dim_attr.write(PFP_TYPE, cellBuffer);

	metadata.close();
};

Array2D<PRISMATIC_FLOAT_PRECISION> readDataSet2D(const std::string &filename, const std::string &dataPath)
{
	H5::H5File input = H5::H5File(filename.c_str(), H5F_ACC_RDONLY);
	H5::DataSet dataset = input.openDataSet(dataPath.c_str());
	H5::DataSpace dataspace = dataset.getSpace();

	hsize_t dims_out[2];
	int ndims = dataspace.getSimpleExtentDims(dims_out, NULL);
	H5::DataSpace mspace(2,dims_out);

	std::vector<PRISMATIC_FLOAT_PRECISION> data_in(dims_out[0]*dims_out[1]);
	dataset.read(&data_in[0], PFP_TYPE, mspace, dataspace);

	mspace.close();
	dataspace.close();
	dataset.close();
	input.close();

	Array2D<PRISMATIC_FLOAT_PRECISION> data = ArrayND<2, std::vector<PRISMATIC_FLOAT_PRECISION>>(data_in, {{dims_out[1], dims_out[0]}});

	return data;
};

Array3D<PRISMATIC_FLOAT_PRECISION> readDataSet3D(const std::string &filename, const std::string &dataPath)
{
	H5::H5File input = H5::H5File(filename.c_str(), H5F_ACC_RDONLY);
	H5::DataSet dataset = input.openDataSet(dataPath.c_str());
	H5::DataSpace dataspace = dataset.getSpace();

	hsize_t dims_out[3];
	int ndims = dataspace.getSimpleExtentDims(dims_out, NULL);
	H5::DataSpace mspace(3,dims_out);

	std::vector<PRISMATIC_FLOAT_PRECISION> data_in(dims_out[0]*dims_out[1]*dims_out[2]);
	dataset.read(&data_in[0], PFP_TYPE, mspace, dataspace);

	mspace.close();
	dataspace.close();
	dataset.close();
	input.close();

	Array3D<PRISMATIC_FLOAT_PRECISION> data = ArrayND<3, std::vector<PRISMATIC_FLOAT_PRECISION>>(data_in, {{dims_out[2], dims_out[1], dims_out[0]}});

	return data;
};

Array4D<PRISMATIC_FLOAT_PRECISION> readDataSet4D(const std::string &filename, const std::string &dataPath)
{
	H5::H5File input = H5::H5File(filename.c_str(), H5F_ACC_RDONLY);
	H5::DataSet dataset = input.openDataSet(dataPath.c_str());
	H5::DataSpace dataspace = dataset.getSpace();

	hsize_t dims_out[4];
	int ndims = dataspace.getSimpleExtentDims(dims_out, NULL);
	H5::DataSpace mspace(4,dims_out);

	std::vector<PRISMATIC_FLOAT_PRECISION> data_in(dims_out[0]*dims_out[1]*dims_out[2]*dims_out[3]);
	dataset.read(&data_in[0], PFP_TYPE, mspace, dataspace);

	mspace.close();
	dataspace.close();
	dataset.close();
	input.close();


	//mem dims are stored 0->3 kx, ky, qx, qy
	//flipping x, y back for storage into 4D array
	Array4D<PRISMATIC_FLOAT_PRECISION> data = ArrayND<4, std::vector<PRISMATIC_FLOAT_PRECISION>>(data_in, {{dims_out[1], dims_out[0], dims_out[3], dims_out[2]}});
	std::array<size_t, 4> dims_in = {dims_out[1], dims_out[0], dims_out[3], dims_out[2]};
	std::array<size_t, 4> order = {1, 0, 3, 2};
	data = restride(data, dims_in, order);

	return data;
};

Array4D<PRISMATIC_FLOAT_PRECISION> readDataSet4D_keepOrder(const std::string &filename, const std::string &dataPath)
{
	//copy of above, but does not move any dimensions around
	H5::H5File input = H5::H5File(filename.c_str(), H5F_ACC_RDONLY);
	H5::DataSet dataset = input.openDataSet(dataPath.c_str());
	H5::DataSpace dataspace = dataset.getSpace();

	hsize_t dims_out[4];
	hsize_t dims_out_switch[4];
	int ndims = dataspace.getSimpleExtentDims(dims_out, NULL);
	dims_out_switch[0] = dims_out[1];
	dims_out_switch[1] = dims_out[2];
	dims_out_switch[2] = dims_out[3];
	dims_out_switch[3] = dims_out[0];
	H5::DataSpace mspace(4,dims_out_switch);

	std::vector<PRISMATIC_FLOAT_PRECISION> data_in(dims_out[0]*dims_out[1]*dims_out[2]*dims_out[3]);
	dataset.read(&data_in[0], PFP_TYPE, mspace, dataspace);

	mspace.close();
	dataspace.close();
	dataset.close();
	input.close();

	Array4D<PRISMATIC_FLOAT_PRECISION> data = ArrayND<4, std::vector<PRISMATIC_FLOAT_PRECISION>>(data_in, {{dims_out[3], dims_out[2], dims_out[1], dims_out[0]}});
	return data;
};

void readAttribute(const std::string &filename, const std::string &groupPath, const std::string &attr, PRISMATIC_FLOAT_PRECISION &val)
{
	//read an attribute from a group into val
	//overloaded by expected value type

	H5::H5File input = H5::H5File(filename.c_str(), H5F_ACC_RDONLY);
	H5::Group group = input.openGroup(groupPath);
	H5::Attribute attribute = group.openAttribute(attr);
	H5::DataType type =  attribute.getDataType();
	attribute.read(type,&val);

	attribute.close();
	group.close();
	input.close();
	
}

void readAttribute(const std::string &filename, const std::string &groupPath, const std::string &attr, PRISMATIC_FLOAT_PRECISION *val)
{
	//read an attribute from a group into val
	//overloaded by expected value type

	H5::H5File input = H5::H5File(filename.c_str(), H5F_ACC_RDONLY);
	H5::Group group = input.openGroup(groupPath);
	H5::Attribute attribute = group.openAttribute(attr);
	H5::DataType type =  attribute.getDataType();
	attribute.read(type,&val[0]);

	attribute.close();
	group.close();
	input.close();
	
}

void readAttribute(const std::string &filename, const std::string &groupPath, const std::string &attr, int &val)
{
	//read an attribute from a group into val
	//overloaded by expected value type

	H5::H5File input = H5::H5File(filename.c_str(), H5F_ACC_RDONLY);
	H5::Group group = input.openGroup(groupPath);
	H5::Attribute attribute = group.openAttribute(attr);
	H5::DataType type =  attribute.getDataType();
	attribute.read(type,&val);

	attribute.close();
	group.close();
	input.close();

}

void readAttribute(const std::string &filename, const std::string &groupPath, const std::string &attr, std::string &val)
{
	//read an attribute from a group into val
	//overloaded by expected value type

	H5::H5File input = H5::H5File(filename.c_str(), H5F_ACC_RDONLY);
	H5::Group group = input.openGroup(groupPath);
	H5::Attribute attribute = group.openAttribute(attr);
	H5::DataType type =  attribute.getDataType();

	H5std_string tmpVal;
	attribute.read(type,tmpVal);
	val = tmpVal;

	attribute.close();
	group.close();
	input.close();

}

void writeComplexDataSet(H5::Group group, const std::string &dsetname, const std::complex<PRISMATIC_FLOAT_PRECISION> *buffer, const hsize_t *mdims, const size_t &rank)
{
	//input buffer is assumed to be float: real, float: im in striding order
	H5::CompType complex_type = H5::CompType(sizeof(complex_float_t));
	const H5std_string re_str("r"); //using h5py default configuration
	const H5std_string im_str("i");
	complex_type.insertMember(re_str, 0, PFP_TYPE);
	complex_type.insertMember(im_str, 4, PFP_TYPE);

	//create dataset and write
	H5::DataSpace mspace(rank, mdims);

	H5::DataSet complex_dset;
	if(group.nameExists(dsetname.c_str()))
	{
		complex_dset = group.openDataSet(dsetname.c_str());
	}
	else
	{
		complex_dset = group.createDataSet(dsetname.c_str(), complex_type, mspace);
	}		
	
	H5::DataSpace fspace = complex_dset.getSpace();
	complex_dset.write(buffer, complex_type, mspace, fspace);

	//close spaces
	fspace.close();
	mspace.close();
	complex_dset.close();

}

int countDataGroups(H5::Group group, const std::string &basename)
{
	//count number of subgroups in specified group with desired basename + "####"
	int count = 0;
	std::string currentName = basename + getDigitString(count);
	while(group.nameExists(currentName.c_str()))
	{
		count += 1;
		currentName = basename + getDigitString(count);
	}

	return count;

};

int countDimensions(H5::Group group, const std::string &basename)
{
	//count number of dimensions in specified group with desired basename + "#"
	//basename used to specified dim or sgdim
	int count = 1;
	std::string currentName = basename + std::to_string(count);
	while(group.nameExists(currentName.c_str()))
	{
		count += 1;
		currentName = basename + std::to_string(count);
	}

	return count-1;

};

void configureSupergroup(H5::Group &new_sg,
						H5::Group &sourceExample,
						const std::vector<std::vector<PRISMATIC_FLOAT_PRECISION>> &sgdims,
						const std::vector<std::string> &sgdims_name,
						const std::vector<std::string> &sgdims_units)
{
	//write group type attribute
	H5::DataSpace attr1_dataspace(H5S_SCALAR);
	H5::Attribute emd_group_type = new_sg.createAttribute("emd_group_type", H5::PredType::NATIVE_INT, attr1_dataspace);
	int group_type = 3;
	emd_group_type.write(H5::PredType::NATIVE_INT, &group_type);

	//write metadata attribute
	H5::DataSpace attr2_dataspace(H5S_SCALAR);
	H5::Attribute metadata_group = new_sg.createAttribute("metadata", H5::PredType::NATIVE_INT, attr2_dataspace);
	int mgroup = 0;
	metadata_group.write(H5::PredType::NATIVE_INT, &mgroup);

	H5::DataSpace str_name_ds(H5S_SCALAR);
	H5::StrType strdatatype(H5::PredType::C_S1, 256);

	//write common dimensions
	int numDims = countDimensions(sourceExample, "dim");
	for(auto i = 0; i < numDims; i++)
	{
		H5::DataSet dim_data = sourceExample.openDataSet("dim"+std::to_string(i+1));
		copyDataSet(new_sg, dim_data);
	}

	//write supergroup dimensions
	for(auto i = 0; i < sgdims.size(); i++)
	{
		hsize_t dim_size[1] = {sgdims[i].size()};
		H5::DataSpace dim_mspace(1, dim_size);
		H5::DataSet dim = new_sg.createDataSet("sgdim"+std::to_string(i+1), PFP_TYPE, dim_mspace);
		H5::DataSpace dim_fspace = dim.getSpace();
		dim.write(&sgdims[i][0], PFP_TYPE, dim_mspace, dim_fspace);

		//dimension attributes
		const H5std_string dim_name_str(sgdims_name[i]);
		const H5std_string dim_unit_str(sgdims_units[i]);
		H5::Attribute dim_name = dim.createAttribute("name", strdatatype, str_name_ds);
		H5::Attribute dim_unit = dim.createAttribute("units", strdatatype, str_name_ds);
		dim_name.write(strdatatype, dim_name_str);
		dim_unit.write(strdatatype, dim_unit_str);
	}

};

void writeVirtualDataSet(H5::Group group,
						const std::string &dsetName,
						std::vector<H5::DataSet> &datasets,
						std::vector<std::vector<size_t>> indices)
{
	//determine new dimensionality of VDS and get extent along each dim
	size_t new_rank = indices[0].size();
	size_t max_dims[new_rank] = {0};
	for(auto i = 0; i < indices.size(); i++)
	{
		for(auto j = 0; j < new_rank; j++)
		{
			max_dims[j] = (max_dims[j] < indices[i][j]) ? indices[i][j] : max_dims[j];
		}
	}

	//organize dimensions and create mem space for virtual dataset
	H5::DataSpace sampleSpace = datasets[0].getSpace();
	int rank = sampleSpace.getSimpleExtentNdims();
	hsize_t dims_out[rank];
	int ndims = sampleSpace.getSimpleExtentDims(dims_out, NULL); //nidms and rank are redundant, but rank is not known a priori
	
	hsize_t mdims[rank+new_rank] = {0};
	hsize_t mdims_ind[rank+new_rank] = {0};

	for(auto i = 0; i < rank; i++)
	{
		mdims[i] = dims_out[i];
		mdims_ind[i] = dims_out[i];
	}

	for(auto i = rank; i < rank+new_rank; i++)
	{
		mdims[i] = max_dims[i-rank]+1;
		mdims_ind[i] = 1;
	}

	sampleSpace.close();

	//create virtual dataspace and plist mapping
	H5::DataSpace vds_mspace(rank+new_rank, mdims);
	H5::DataSpace src_mspace;
	H5::DSetCreatPropList plist;
    std::string path;
	hsize_t offset[rank+new_rank] = {0};
	
	for(auto i = 0; i < datasets.size(); i++)
	{
		path = datasets[i].getObjName();
		src_mspace = datasets[i].getSpace();

		for(auto j = rank; j < rank+new_rank; j++) offset[j] = indices[i][j-rank];
		
		vds_mspace.selectHyperslab(H5S_SELECT_SET, mdims_ind, offset);
		plist.setVirtual(vds_mspace, datasets[i].getFileName(), path, src_mspace);
	}
	//reset offset
	for(auto i = rank; i < rank+new_rank; i++) offset[i] = 0;

	vds_mspace.selectHyperslab(H5S_SELECT_SET, mdims, offset);
	H5::DataSet vds = group.createDataSet(dsetName, datasets[0].getDataType(), vds_mspace, plist);

	src_mspace.close();
	vds.close();

};

void depthSeriesSG(H5::H5File &file)
{
	H5::Group supergroups = file.openGroup("4DSTEM_simulation/data/supergroups");
	H5::Group depthSeries = supergroups.createGroup("vd_depth_series");

	//gather datasets and create mapping
	std::string basename = "virtual_detector_depth";
	H5::Group realslices = file.openGroup("4DSTEM_simulation/data/realslices");

	int numDataSets = countDataGroups(realslices, basename);
	std::vector<H5::DataSet> datasets;
	std::vector<std::vector<size_t>> indices;

	std::vector<PRISMATIC_FLOAT_PRECISION> depths;
	PRISMATIC_FLOAT_PRECISION tmp_depth;
	for(auto i = 0; i < numDataSets; i++)
	{
		std::string tmp_name = basename+getDigitString(i);
		H5::Group tmp_group = realslices.openGroup(tmp_name.c_str());
		H5::DataSet tmp_dataset = tmp_group.openDataSet("realslice");
		datasets.push_back(tmp_dataset);
		indices.push_back(std::vector<size_t>{i});
		readAttribute(tmp_group.getFileName(), tmp_group.getObjName(), "output_depth", tmp_depth);
		depths.push_back(tmp_depth);
	}

	//write dataset
	writeVirtualDataSet(depthSeries, "supergroup", datasets, indices);

	//configure supergroup
	//gather dim properties from first datagroup and write sgdims
	H5::Group firstGroup = realslices.openGroup(basename+getDigitString(0));
	std::vector<std::vector<PRISMATIC_FLOAT_PRECISION>> sgdims;
	sgdims.push_back(depths);
	
	std::vector<std::string> sgdims_name;
	sgdims_name.push_back("Depth");

	std::vector<std::string> sgdims_units;
	sgdims_units.push_back("[Å]");

	configureSupergroup(depthSeries, firstGroup, sgdims, sgdims_name, sgdims_units);

}

std::string reducedDataSetName(std::string &fullPath)
{
	size_t index = fullPath.find_last_of("/");
	return fullPath.substr(index+1);
}

void copyDataSet(H5::Group &targetGroup, H5::DataSet &source)
{
	//grab properties from source dataset
	std::string dsName = source.getObjName();
	dsName = reducedDataSetName(dsName);

	H5::DataSpace sourceSpace = source.getSpace();
	int rank = sourceSpace.getSimpleExtentNdims();
	hsize_t dims_out[rank];
	int ndims = sourceSpace.getSimpleExtentDims(dims_out, NULL); //nidms and rank are redundant, but rank is not known a priori

	//create buffer array and read data from source
	H5::DataSpace mspace(rank, dims_out);
	size_t bufferSize = source.getInMemDataSize(); //size in bytes
	unsigned char buffer[bufferSize]; //unsigned char is always byte sized, let's us be agnostic to storage type of array
	source.read(buffer, source.getDataType(), mspace, sourceSpace);	

	//create new 
	H5::DataSet target = targetGroup.createDataSet(dsName.c_str(), source.getDataType(), mspace);
	H5::DataSpace fspace = target.getSpace();
	target.write(&buffer[0], source.getDataType(), mspace, fspace);

	//look for attributes in source and copy
	for(auto i = 0; i < source.getNumAttrs(); i++)
	{
		H5::Attribute tmp_attr = source.openAttribute(i);
		H5::DataSpace attr_space = tmp_attr.getSpace();
		int attr_rank = attr_space.getSimpleExtentNdims();
		hsize_t attr_dims_out[attr_rank];
		int ndims = attr_space.getSimpleExtentDims(attr_dims_out, NULL); //nidms and rank are redundant, but rank is not known a priori
		
		H5::DataSpace t_attr_space(attr_rank, attr_dims_out);
		H5::Attribute t_attr = target.createAttribute(tmp_attr.getName(), tmp_attr.getDataType(), t_attr_space);
		
		unsigned char attr_buffer[tmp_attr.getInMemDataSize()];
		tmp_attr.read(tmp_attr.getDataType(), attr_buffer);
		t_attr.write(tmp_attr.getDataType(), &attr_buffer[0]);
	}

	target.close();


};

} //namespace Prismatic