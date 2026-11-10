namespace Camber.Host;
internal static class TestDataPaths {
    public static readonly string Root=Path.GetFullPath(Environment.GetEnvironmentVariable("CAMBER_ISOLATED_TEST_DATA")
        ?? throw new InvalidOperationException("An explicit isolated test data folder is required."));
}
