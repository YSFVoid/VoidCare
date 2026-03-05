namespace VoidCare.Wpf.Services;

public interface INavigationService
{
    string CurrentPage { get; }
    event EventHandler<string>? PageChanged;
    void Navigate(string page);
}
