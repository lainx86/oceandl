#include "builtin_datasets.hpp"

namespace oceandl {

const std::vector<BuiltinDatasetSpec>& builtin_dataset_specs() {
    static const std::vector<BuiltinDatasetSpec> specs{
        {
            .id = "oisst",
            .display_name = "NOAA OISST Daily Mean",
            .description =
                "Daily mean sea surface temperature NetCDF files from NOAA Optimum "
                "Interpolation SST V2.1.",
            .provider_key = "psl",
            .default_path = "Datasets/noaa.oisst.v2.highres",
            .filename_pattern = "sst.day.mean.{year}.nc",
            .file_mode = FileMode::PerYear,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = 1981,
            .end_year = std::nullopt,
        },
        {
            .id = "gpcp",
            .display_name = "GPCP Monthly Precipitation",
            .description =
                "Global monthly precipitation estimates from the Global Precipitation "
                "Climatology Project.",
            .provider_key = "psl",
            .default_path = "Datasets/gpcp",
            .filename_pattern = "precip.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "air",
            .display_name = "NCEP Reanalysis Air Temperature",
            .description =
                "Monthly mean near-surface air temperature from the NCEP/NCAR Reanalysis.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis.derived/surface",
            .filename_pattern = "air.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "mslp",
            .display_name = "NCEP Reanalysis Mean Sea Level Pressure",
            .description =
                "Monthly mean sea level pressure from the NCEP/NCAR Reanalysis.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis.derived/surface",
            .filename_pattern = "mslp.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "uwnd_surface",
            .display_name = "NCEP Reanalysis Surface Zonal Wind",
            .description =
                "Monthly mean near-surface zonal wind from the NCEP/NCAR Reanalysis.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis.derived/surface",
            .filename_pattern = "uwnd.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "vwnd_surface",
            .display_name = "NCEP Reanalysis Surface Meridional Wind",
            .description =
                "Monthly mean near-surface meridional wind from the NCEP/NCAR Reanalysis.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis.derived/surface",
            .filename_pattern = "vwnd.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "rhum_surface",
            .display_name = "NCEP Reanalysis Surface Relative Humidity",
            .description =
                "Monthly mean near-surface relative humidity from the NCEP/NCAR Reanalysis.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis.derived/surface",
            .filename_pattern = "rhum.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "pr_wtr",
            .display_name = "NCEP Reanalysis Precipitable Water",
            .description =
                "Monthly mean precipitable water from the NCEP/NCAR Reanalysis.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis.derived/surface",
            .filename_pattern = "pr_wtr.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "hgt_pressure",
            .display_name = "NCEP Reanalysis 2 Geopotential Height",
            .description =
                "Monthly mean geopotential height on pressure levels from the NCEP/DOE Reanalysis 2.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis2.derived/pressure",
            .filename_pattern = "hgt.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
        {
            .id = "omega_pressure",
            .display_name = "NCEP Reanalysis 2 Vertical Velocity",
            .description =
                "Monthly mean vertical velocity on pressure levels from the NCEP/DOE Reanalysis 2.",
            .provider_key = "psl",
            .default_path = "Datasets/ncep.reanalysis2.derived/pressure",
            .filename_pattern = "omega.mon.mean.nc",
            .file_mode = FileMode::SingleFile,
            .payload_format = DatasetPayloadFormat::Netcdf,
            .start_year = std::nullopt,
            .end_year = std::nullopt,
        },
    };
    return specs;
}

std::map<std::string, std::string> builtin_provider_base_urls() {
    return {
        {"psl", "https://downloads.psl.noaa.gov"},
    };
}

std::map<std::string, std::string> builtin_dataset_base_urls() {
    const auto provider_base_urls = builtin_provider_base_urls();
    std::map<std::string, std::string> urls;
    for (const auto& spec : builtin_dataset_specs()) {
        const auto provider_base_url = provider_base_urls.at(std::string(spec.provider_key));
        urls.emplace(
            std::string(spec.id),
            provider_base_url + "/" + std::string(spec.default_path)
        );
    }
    return urls;
}

}  // namespace oceandl
