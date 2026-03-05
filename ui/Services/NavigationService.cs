namespace VoidCare.Wpf.Services;

public sealed class NavigationService : INavigationService
{
    private string _currentPage = "Dashboard";

    public string CurrentPage => _currentPage;

    public event EventHandler<string>? PageChanged;

    public void Navigate(string page)
    {
        if (string.Equals(_currentPage, page, StringComparison.OrdinalIgnoreCase))
        {
            return;
        }

        _currentPage = page;
        PageChanged?.Invoke(this, page);
    }
}
