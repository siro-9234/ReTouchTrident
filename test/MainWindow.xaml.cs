using System;
using System.Windows;
using user;

namespace test
{
    public partial class MainWindow : Window
    {
        private DriverClient? _driver;

        public MainWindow()
        {
            InitializeComponent();
        }

        private void Open_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                _driver = new DriverClient();
                LogText.Text = "Driver opened.";
            }
            catch (Exception ex)
            {
                LogText.Text = ex.Message;
            }
        }

        private void Down_Click(object sender, RoutedEventArgs e)
        {
            _driver?.TouchDown(0, 20000, 20000);
            _driver?.Commit();
            LogText.Text = "TouchDown sent.";
        }

        private void Move_Click(object sender, RoutedEventArgs e)
        {
            _driver?.TouchMove(0, 30000, 25000);
            _driver?.Commit();
            LogText.Text = "TouchMove sent.";
        }

        private void Up_Click(object sender, RoutedEventArgs e)
        {
            _driver?.TouchUp(0);
            _driver?.Commit();
            LogText.Text = "TouchUp sent.";
        }
    }
}